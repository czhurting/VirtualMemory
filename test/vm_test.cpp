// vm::UniqueVirtual unit test: verifies commit/decommit free-list coalescing
// Define VM_TEST at build time to expose UniqueVirtual::free_list() (see UniqueMemory.hpp)
#define VM_TEST
#include <UniqueMemory.hpp>
#include <cstdio>
#include <cstddef>
#include <vector>
#include <random>
#include <chrono>

using namespace vm;

static int g_fail = 0;

static void check(bool cond, const char* msg)
{
	if (!cond) { std::printf("  FAIL: %s\n", msg); ++g_fail; }
	else       { std::printf("  PASS: %s\n", msg); }
}

// Compare free list against expected set of [start,end) page ranges
static void expect_free(const UniqueVirtual& v,
                        const std::vector<std::pair<std::size_t, std::size_t>>& expected,
                        const char* msg)
{
	const auto& fl = v.free_list();
	bool ok = (fl.size() == expected.size());
	if (ok)
		for (std::size_t i = 0; i < fl.size(); ++i)
			if (fl[i].first != expected[i].first || fl[i].second != expected[i].second) { ok = false; break; }

	if (!ok)
	{
		std::printf("  FAIL: %s\n    got: ", msg);
		for (auto& p : fl) std::printf("[%zu,%zu) ", p.first, p.second);
		std::printf("\n    exp: ");
		for (auto& p : expected) std::printf("[%zu,%zu) ", p.first, p.second);
		std::printf("\n");
		++g_fail;
	}
	else
	{
		std::printf("  PASS: %s\n", msg);
	}
}

int main()
{
	const std::size_t PG = PageSize();

	// --- Case A: commit then decommit everything, expect full reclaim ---
	{
		std::printf("[A] full reclaim\n");
		UniqueVirtual v(8 * PG);
		auto a = v.Commit(0, 4 * PG);
		expect_free(v, { {4, 8} }, "after committing 4 pages, free=[4,8)");
		v.Decommit(a.offset, 4 * PG);
		expect_free(v, { {0, 8} }, "after decommit, free=[0,8)");
	}

	// --- Case B: scattered commits then alternating decommits, single-side merge ---
	{
		std::printf("[B] single-side merge\n");
		UniqueVirtual v(8 * PG);
		auto b0 = v.Commit(0, PG);
		auto b1 = v.Commit(0, PG);
		auto b2 = v.Commit(0, PG);
		auto b3 = v.Commit(0, PG);
		expect_free(v, { {4, 8} }, "4x1 page committed, free=[4,8)");

		v.Decommit(b1.offset, PG);
		expect_free(v, { {1, 2}, {4, 8} }, "decommit page 1 -> [1,2),[4,8)");
		v.Decommit(b3.offset, PG);
		expect_free(v, { {1, 2}, {3, 8} }, "decommit page 3 -> merges with right [4,8) -> [1,2),[3,8)");
	}

	// --- Case C: sandwich merge (both sides coalesce) ---
	{
		std::printf("[C] sandwich merge\n");
		UniqueVirtual v(8 * PG);
		auto b0 = v.Commit(0, PG);
		auto b1 = v.Commit(0, PG);
		auto b2 = v.Commit(0, PG);
		auto b3 = v.Commit(0, PG);
		v.Decommit(b1.offset, PG);
		v.Decommit(b3.offset, PG);
		v.Decommit(b2.offset, PG);
		expect_free(v, { {1, 8} }, "sandwich decommit -> [1,8)");
	}

	// --- Case D: double decommit must throw ---
	{
		std::printf("[D] double decommit throws\n");
		UniqueVirtual v(8 * PG);
		auto a = v.Commit(0, 4 * PG);
		v.Decommit(a.offset, 4 * PG);
		bool threw = false;
		try { v.Decommit(a.offset, 4 * PG); }
		catch (...) { threw = true; }
		check(threw, "decommitting an already-free region must throw");
	}

	// --- Case E: random fuzz: commit/decommit keeps free-list invariant ---
	{
		std::printf("[E] random fuzz\n");
		std::random_device rd{};
		std::mt19937_64 gen(rd());
		const std::size_t PG = PageSize();
		const std::size_t NPAGES = 64;
		UniqueVirtual v(NPAGES * PG);
		std::vector<std::size_t> committed;
		bool ok = true;
		for (std::size_t iter = 0; iter < 500; ++iter)
		{
			bool can_commit = committed.size() < NPAGES;
			std::uniform_int_distribution<int> act(0, 1);
			bool do_commit = committed.empty() || (can_commit && act(gen) == 0);
			if (do_commit)
			{
				auto blk = v.Commit(0, PG);
				committed.push_back(blk.offset);
			}
			else
			{
				std::uniform_int_distribution<std::size_t> idx(0, committed.size() - 1);
				std::size_t k = idx(gen);
				v.Decommit(committed[k], PG);
				committed.erase(committed.begin() + k);
			}
			const auto& fl = v.free_list();
			std::size_t free_pages = 0;
			for (auto& p : fl) free_pages += (p.second - p.first);
			if (free_pages + committed.size() != NPAGES) { ok = false; break; }
		}
		check(ok, "random fuzz: free-list invariant held over 500 random ops");
	}

	// --- Case F: stress test - large region + many random ops, with timing ---
	{
		std::printf("[F] stress fuzz\n");
		std::random_device rd{};
		std::mt19937_64 gen(rd());
		const std::size_t PG = PageSize();
		const std::size_t NPAGES = 4096;
		UniqueVirtual v(NPAGES * PG);
		std::vector<std::size_t> committed;
		committed.reserve(NPAGES);
		bool ok = true;
		const std::size_t ITERS = 10000000;
		auto t0 = std::chrono::steady_clock::now();
		for (std::size_t iter = 0; iter < ITERS; ++iter)
		{
			bool can_commit = committed.size() < NPAGES;
			std::uniform_int_distribution<int> act(0, 1);
			bool do_commit = committed.empty() || (can_commit && act(gen) == 0);
			if (do_commit)
			{
				auto blk = v.Commit(0, PG);
				committed.push_back(blk.offset);
			}
			else
			{
				std::uniform_int_distribution<std::size_t> idx(0, committed.size() - 1);
				std::size_t k = idx(gen);
				v.Decommit(committed[k], PG);
				committed.erase(committed.begin() + k);
			}
			// 抽样校验，降低开销：每 4096 轮验一次不变量
			if ((iter & 4095) == 0)
			{
				const auto& fl = v.free_list();
				std::size_t free_pages = 0;
				for (auto& p : fl) free_pages += (p.second - p.first);
				if (free_pages + committed.size() != NPAGES) { ok = false; break; }
			}
		}
		auto t1 = std::chrono::steady_clock::now();
		double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
		const auto& fl = v.free_list();
		std::size_t free_pages = 0;
		for (auto& p : fl) free_pages += (p.second - p.first);
		if (free_pages + committed.size() != NPAGES) ok = false;
		std::printf("  (%.1f ms for %zu ops, ~%.0f ops/ms)\n", ms, ITERS, ITERS / ms);
		check(ok, "stress fuzz: free-list invariant held under 10M random ops");
	}

	if (g_fail == 0) { std::printf("\nALL TESTS PASSED\n"); return 0; }
	std::printf("\n%d TEST(S) FAILED\n", g_fail);
	return 1;
}
