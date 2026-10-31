// Copyright (c) 2020 Nicholas Corgan
// SPDX-License-Identifier: BSL-1.0

#include "MemoryMappedBufferContainer.hpp"

#include <Pothos/Exception.hpp>
#include <Pothos/Util/ExceptionForErrorCode.hpp>

#include "Util/FileSystem.hpp"
#include <filesystem>
#include <Pothos/System/Logger.hpp>
#include <cstring> //strerror


#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

template <typename ExcType = Pothos::RuntimeException>
static inline void throwErrnoOnFailure(int code)
{
    if(code < 0) throw Pothos::Util::ErrnoException<ExcType>(code);
}

static void logErrnoOnFailure(int code, const char* context)
{
    if(code < 0)
    {
        POTHOS_LOG_ERROR("MemoryMappedBufferContainer", "{}: {}",
            std::string(context),
            std::string(::strerror(errno)));
    }
}

class MemoryMappedBufferContainer::Impl
{
public:
    Impl(const std::string& filepath,
         bool readable,
         bool writable): _buffer(nullptr), _length(0)
    {
        int openFlags = 0;
        if(readable && writable) openFlags = O_RDWR;
        else if(readable)        openFlags = O_RDONLY;
        else if(writable)        openFlags = O_WRONLY;

        int mmapProt = 0;
        if(readable) mmapProt |= PROT_READ;
        if(writable) mmapProt |= PROT_WRITE;

        constexpr int mmapFlags = MAP_SHARED;

        _length = std::filesystem::file_size(Pothos::Util::Detail::utf8Path(filepath));

        int fd = 0;
        throwErrnoOnFailure<Pothos::OpenFileException>(
            (fd = ::open(filepath.c_str(), openFlags)));

        _buffer = ::mmap(nullptr, _length, mmapProt, mmapFlags, fd, 0);
        if(!_buffer || (MAP_FAILED == _buffer))
        {
            throw Pothos::IOException("mmap", ::strerror(errno));
        }

        throwErrnoOnFailure<Pothos::FileException>(::close(fd));
    }

    ~Impl()
    {
        logErrnoOnFailure(
            ::munmap(_buffer, _length),
            "munmap");
    }

    void* buffer() const
    {
        return _buffer;
    }

    size_t length() const
    {
        return _length;
    }

private:
    void* _buffer;
    size_t _length;
};

#else
//
// Windows
//

#include "Platform/Process.hpp" //utf8ToWide
#include <Windows.h>
#include <system_error>

template <typename ExcType = Pothos::RuntimeException>
static void throwWindowsErrorOnFailure(DWORD code)
{
    if(code) throw Pothos::Util::WinErrorException<ExcType>(code);
}

static void logWindowsErrorOnFailure(DWORD code, const char* context)
{
    if(code)
    {
        POTHOS_LOG_ERROR("MemoryMappedBufferContainer", "{}: {}",
            std::string(context),
            std::system_category().message(code));
    }
}

class MemoryMappedBufferContainer::Impl
{
public:
    Impl(const std::string& filepath,
        bool readable,
        bool writable) : _buffer(nullptr), _length(0), _fileHandle(nullptr)
    {
        _length = std::filesystem::file_size(Pothos::Util::Detail::utf8Path(filepath));

        //
        // Variables used by multiple functions
        //

        constexpr ::LPSECURITY_ATTRIBUTES pSecurityAttributes = nullptr;

        //
        // Open file and store handle
        //

        DWORD createFileDesiredAccess = 0;
        if (readable) createFileDesiredAccess |= GENERIC_READ;
        if (writable) createFileDesiredAccess |= GENERIC_WRITE;

        constexpr DWORD shareMode = 0; // Don't let other processes mess with this file
        constexpr DWORD creationDisposition = OPEN_ALWAYS; // Only support existing files
        constexpr DWORD flagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
        constexpr ::HANDLE templateFile = nullptr;

        //the path is UTF-8: the wide API (CreateFileA would use the ANSI code page)
        const auto widePath = Pothos::Platform::utf8ToWide(filepath);
        _fileHandle = ::CreateFileW(
                          widePath.c_str(),
                          createFileDesiredAccess,
                          shareMode,
                          pSecurityAttributes,
                          creationDisposition,
                          flagsAndAttributes,
                          templateFile);
        if (INVALID_HANDLE_VALUE == _fileHandle)
        {
            const DWORD createFileErrorCode = ::GetLastError();

            this->_closeHandle(_fileHandle);
            throwWindowsErrorOnFailure(createFileErrorCode);
        }

        //
        // Create file mapping and store handle
        //

        DWORD fileProtection = 0;
        if (readable && writable) fileProtection = PAGE_READWRITE;
        else if (readable)        fileProtection = PAGE_READONLY;
        else if (writable)        fileProtection = PAGE_WRITECOPY;

        // This function takes the file size in through two parameters.
        DWORD maxSizeHigh = static_cast<DWORD>(_length >> 32);
        DWORD maxSizeLow = static_cast<DWORD>(_length & 0xFFFFFFFF);

        constexpr LPCWSTR name = nullptr;

        _mappingHandle = ::CreateFileMappingW(
                             _fileHandle,
                             pSecurityAttributes,
                             fileProtection,
                             maxSizeHigh,
                             maxSizeLow,
                             name);
        if (INVALID_HANDLE_VALUE == _mappingHandle)
        {
            const DWORD createFileMappingErrorCode = ::GetLastError();

            this->_closeHandle(_mappingHandle);
            this->_closeHandle(_fileHandle);
            throwWindowsErrorOnFailure(createFileMappingErrorCode);
        }

        //
        // Map our buffer pointer to the file's contents.
        //

        DWORD mapViewOfFileDesiredAccess = 0;
        if (readable && writable) mapViewOfFileDesiredAccess = FILE_MAP_ALL_ACCESS;
        else if (readable)        mapViewOfFileDesiredAccess = FILE_MAP_READ;
        else if (writable)        mapViewOfFileDesiredAccess = FILE_MAP_WRITE;

        constexpr DWORD offsetHigh = 0;
        constexpr DWORD offsetLow = 0;

        _buffer = ::MapViewOfFile(
                       _mappingHandle,
                       mapViewOfFileDesiredAccess,
                       offsetHigh,
                       offsetLow,
                       _length);
        if (!_buffer)
        {
            const DWORD mapViewOfFileErrorCode = ::GetLastError();

            this->_closeHandle(_mappingHandle);
            this->_closeHandle(_fileHandle);
            throwWindowsErrorOnFailure(mapViewOfFileErrorCode);
        }
    }

    ~Impl()
    {
        if (!::UnmapViewOfFile(_buffer))
        {
            logWindowsErrorOnFailure(::GetLastError(), "UnmapViewOfFile()");
        }

        this->_closeHandle(_mappingHandle);
        this->_closeHandle(_fileHandle);
    }

    void* buffer() const
    {
        return _buffer;
    }

    size_t length() const
    {
        return _length;
    }

private:
    void* _buffer;
    size_t _length;
    ::HANDLE _fileHandle;
    ::HANDLE _mappingHandle;

    void _closeHandle(::HANDLE handle)
    {
        // This is called in the destructor and when the file is invalid. In either event, just
        // log the error.
        if (!::CloseHandle(handle))
        {
            logWindowsErrorOnFailure(::GetLastError(), "CloseHandle()");
        }
    }
};
#endif

//
// OS-independent
//

MemoryMappedBufferContainer::SPtr MemoryMappedBufferContainer::make(
    const std::string& filepath,
    bool readable,
    bool writable)
{
    return std::make_shared<MemoryMappedBufferContainer>(filepath, readable, writable);
}

MemoryMappedBufferContainer::MemoryMappedBufferContainer(
    const std::string& filepath,
    bool readable,
    bool writable): _implUPtr(nullptr)
{
    using Pothos::Util::Detail::canAccess;
    using Pothos::Util::Detail::Access;
    if (!std::filesystem::exists(Pothos::Util::Detail::utf8Path(filepath))) throw Pothos::FileNotFoundException(filepath);
    if (std::filesystem::file_size(Pothos::Util::Detail::utf8Path(filepath)) == 0) throw Pothos::InvalidArgumentException("Empty files not supported", filepath);
    if (readable && !canAccess(Pothos::Util::Detail::utf8Path(filepath), Access::Read)) throw Pothos::FileAccessDeniedException(filepath);
    if (writable && !canAccess(Pothos::Util::Detail::utf8Path(filepath), Access::Write)) throw Pothos::FileReadOnlyException(filepath);

    _implUPtr.reset(new Impl(filepath, readable, writable));
}

MemoryMappedBufferContainer::~MemoryMappedBufferContainer()
{
}

void* MemoryMappedBufferContainer::buffer() const
{
    return _implUPtr->buffer();
}

size_t MemoryMappedBufferContainer::length() const
{
    return _implUPtr->length();
}
