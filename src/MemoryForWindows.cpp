#include <Memory.hpp>
#include <Windows.h>

namespace VirtualMemory
{
    static SYSTEM_INFO getSystemInfo()
    {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        return info;
    }
    size_t PageSize()
    {
        static auto si=getSystemInfo();
        return si.dwPageSize;
    }


    /*内存保护权限*/
    Permission operator|(Permission lhs, Permission rhs)
    {
        return static_cast<Permission>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
    }
    Permission operator&(Permission lhs, Permission rhs)
    {
        return static_cast<Permission>(static_cast<std::uint8_t>(lhs) & static_cast<std::uint8_t>(rhs));
    }

    /*UniqueVirtual*/
    UniqueVirtual::UniqueVirtual(std::size_t size, void* hint = nullptr):total_size(size)
    {
        base=VirtualAlloc(hint,size,MEM_RESERVE,PAGE_EXECUTE_READWRITE,hint);
        if(base==nullptr)
        {
            throw std::bad_alloc();
        }
    }
    UniqueVirtual::UniqueVirtual(UniqueVirtual&& other) noexcept:total_size(other.total_size),base(other.base),committed_size(other.committed_size)
    {
        other.base=nullptr;
    }
    UniqueVirtual& UniqueVirtual::operator=(UniqueVirtual&& other) noexcept
    {
        if(this==&other) return *this;
        if(base)
        {
            VirtualFree(base,total_size,MEM_RELEASE);
        }
        committed_size=other.committed_size;
        other.committed_size=0;
        base=other.base;
        total_size=other.total_size;
        other.base=nullptr;
        other.total_size=0;
        return *this;
    }
    void UniqueVirtual::CommitAll()
    {
        if(committed_size==total_size) return;
        if(!VirtualAlloc(base,total_size-committed_size,MEM_COMMIT,PAGE_EXECUTE_READWRITE)==nullptr)
        {
            throw std::bad_alloc();
        }
        committed_size=total_size;
    }
    void UniqueVirtual::DecommitAll()
    {
        if(committed_size==0) return;
        if(!VirtualFree(base,total_size-committed_size,MEM_DECOMMIT))
        {
            throw std::bad_alloc();
        }
        committed_size=0;
    }
    void UniqueVirtual::CommitRange(size_t offset, size_t bytes)
    {
        if(committed_size>=offset+bytes) throw std::out_of_range("Range out of bounds");
        if(!VirtualAlloc(base+offset,bytes,MEM_COMMIT,PAGE_EXECUTE_READWRITE))
        {
            throw std::bad_alloc();
        }
        committed_size+=bytes;
    }
    void UniqueVirtual::DecommitRange(size_t offset, size_t bytes)
    {
        if(committed_size<=offset) throw std::out_of_range("Range out of bounds");
        if(!VirtualFree(base+offset,bytes,MEM_DECOMMIT))
        {
            throw std::bad_alloc();
        }
        committed_size-=bytes;
    }
    void UniqueVirtual::EnsureRange(size_t offset, size_t bytes)
    {
        if(committed_size>=offset+bytes) throw std::out_of_range("Range out of bounds");
        if(!VirtualAlloc(base+offset,bytes,MEM_COMMIT,PAGE_EXECUTE_READWRITE)==nullptr)
        {
            throw std::bad_alloc();
        }
        committed_size+=bytes;
    }
    void *UniqueVirtual::Release() noexcept
    {
        void* base;
        base=nullptr;
        total_size=0;
        committed_size=0;
        return base;
    }

    UniqueVirtual::~UniqueVirtual()
    {
        if(!base) return;
        VirtualFree(base,0,MEM_RELEASE);
    }
    
    /*UniqueCommit*/
    UniqueCommit::UniqueCommit(std::size_t size, void* hint = nullptr):size(size)
    {
        data=VirtualAlloc(hint,size,MEM_COMMIT,PAGE_EXECUTE_READWRITE,hint);
        if(data==nullptr)
        {
            throw std::bad_alloc();
        }
    }
    UniqueCommit::UniqueCommit(UniqueCommit&& other) noexcept:size(other.size),data(other.data),committed_size(other.committed_size)
    {
        other.data=nullptr;
    }
    UniqueCommit& UniqueCommit::operator=(UniqueCommit&& other) noexcept
    {
        if(this==&other) return *this;
        if(data)
        {
            VirtualFree(data,0,MEM_RELEASE);
        }
        committed_size=other.committed_size;
        other.committed_size=0;
        data=other.data;
        size=other.size;
        other.data=nullptr;
        size=0;
        return *this;
    }

    UniqueCommit::~UniqueCommit()
    {
        if(!data) return;
        VirtualFree(data,0,MEM_RELEASE);
    }
}