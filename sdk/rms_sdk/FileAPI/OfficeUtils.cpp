/*
 * ======================================================================
 * Copyright (c) Microsoft Open Technologies, Inc.  All rights reserved.
 * Licensed under the MIT License.
 * See LICENSE.md in the project root for license information.
 * ======================================================================
 */

#include "OfficeUtils.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <vector>
#include "RMSExceptions.h"
#include "../Common/CommonTypes.h"
#include "../Platform/Logger/Logger.h"

using namespace rmscore::platform::logger;
using namespace rmscore::common;

namespace rmscore {
namespace fileapi {

void WriteStreamHeader(GsfOutput* stm, const uint64_t& contentLength)
{
    if ( stm == nullptr)
    {
        Logger::Error("Invalid arguments provided for writing stream header");
        throw exceptions::RMSLogicException(exceptions::RMSException::ErrorTypes::StreamError,
                                            "Error in writing to stream");
    }
    gsf_output_seek(stm, 0, G_SEEK_SET);
    gsf_output_write(stm, sizeof(uint64_t), reinterpret_cast<const uint8_t*>(&contentLength));
}

void ReadStreamHeader(GsfInput* stm, uint64_t& contentLength)
{
    if ( stm == nullptr)
    {
        Logger::Error("Invalid arguments provided for reading stream header");
        throw exceptions::RMSLogicException(exceptions::RMSException::ErrorTypes::StreamError,
                                            "Error in reading from stream");
    }
    gsf_input_seek(stm, 0, G_SEEK_SET);
    gsf_input_read(stm, sizeof(uint64_t), reinterpret_cast<uint8_t*>(&contentLength));
}

modernapi::UserPolicyCreationOptions ConvertToUserPolicyCreationOptions(
        const bool& allowAuditedExtraction,
        CryptoOptions cryptoOptions)
{
    auto userPolicyCreationOptions = allowAuditedExtraction ?
                modernapi::UserPolicyCreationOptions::USER_AllowAuditedExtraction :
                modernapi::UserPolicyCreationOptions::USER_None;
    if (cryptoOptions == CryptoOptions::AUTO ||
            cryptoOptions == CryptoOptions::AES128_ECB )
    {
        userPolicyCreationOptions = static_cast<modernapi::UserPolicyCreationOptions>(
                    userPolicyCreationOptions |
                    modernapi::UserPolicyCreationOptions::USER_PreferDeprecatedAlgorithms);
    }
    else    //temporary until we have CBC for office files
    {
        throw exceptions::RMSLogicException(exceptions::RMSException::ErrorTypes::NotSupported,
                                            "CBC Encryption with Office files is not yet"
                                            "supported");
    }
    return userPolicyCreationOptions;
}

void CopyFromFileToOstream(FILE* file, std::ostream* stream)
{
    fseek(file, 0L, SEEK_END);
    uint64_t fileSize = ftell(file);
    rewind(file);
    stream->seekp(0);
    std::vector<uint8_t> buffer(BUF_SIZE);
    auto count = fileSize;
    while(count > BUF_SIZE)
    {
        fread(&buffer[0], BUF_SIZE, 1, file);
        stream->write(reinterpret_cast<const char*>(buffer.data()), BUF_SIZE);
        count -= BUF_SIZE;
    }

    fread((&buffer[0]), count, 1, file);
    stream->write(reinterpret_cast<const char*>(buffer.data()), count);
    stream->flush();
}

void CopyFromIstreamToFile(std::istream* stream , const std::string& tempFileName,
                           uint64_t inputFileSize)
{
    std::unique_ptr<FILE, FILE_deleter> tempFile(fopen(tempFileName.c_str(), "w+b"));

    stream->seekg(0);
    fseek(tempFile.get(), 0L, SEEK_SET);
    std::vector<uint8_t> buffer(BUF_SIZE);
    auto count = inputFileSize;
    while(count > BUF_SIZE)
    {
        stream->read(reinterpret_cast<char *>(&buffer[0]), BUF_SIZE);
        fwrite(reinterpret_cast<const char*>(buffer.data()), BUF_SIZE, 1, tempFile.get());
        count -= BUF_SIZE;
    }

    stream->read(reinterpret_cast<char *>(&buffer[0]), count);
    fwrite(reinterpret_cast<const char*>(buffer.data()), count, 1, tempFile.get());
    fflush(tempFile.get());
}

std::string CreateTemporaryFileName(const std::string& fileName)
{
    srand(time(NULL));
    uint32_t random = rand() % 10000;
    return (fileName + std::to_string(random) + ".tmp");
}

uint64_t GetFileSize(std::istream* stream, uint64_t maxFileSize)
{
    stream->seekg(0, std::ios::end);
    uint64_t fileSize = stream->tellg();
    stream->seekg(0);
    if(maxFileSize < fileSize)
    {
        Logger::Error("Input file too large");
        throw exceptions::RMSLogicException(exceptions::RMSLogicException::ErrorTypes::NotSupported,
                                            "The file is too large. The limit is 1GB for encryption"
                                            "and 3GB for decryption");
    }

    return fileSize;
}

uint64_t GetFileSize(FILE* file, uint64_t maxFileSize)
{
    fseek(file, 0L, SEEK_END);
    uint64_t fileSize = ftell(file);
    rewind(file);
    if(maxFileSize < fileSize)
    {
        Logger::Error("Input file too large");
        throw exceptions::RMSLogicException(exceptions::RMSLogicException::ErrorTypes::NotSupported,
                                            "The file is too large. The limit is 1GB for encryption"
                                            "and 3GB for decryption");
    }

    return fileSize;
}

} // namespace fileapi
} // namespace rmscore
