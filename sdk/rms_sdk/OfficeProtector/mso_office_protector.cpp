/*
 * ======================================================================
 * Copyright (c) Microsoft Open Technologies, Inc.  All rights reserved.
 * Licensed under the MIT License.
 * See LICENSE.md in the project root for license information.
 * ======================================================================
*/

#include "mso_office_protector.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <vector>
#include <gsf/gsf.h>
#include "BlockBasedProtectedStream.h"
#include "CryptoAPI.h"
#include "file_api_structures.h"
#include "RMSExceptions.h"
#include "UserPolicy.h"
#include "../Common/CommonTypes.h"
#include "../Core/ProtectionPolicy.h"
#include "../PFile/utils.h"
#include "../Platform/Logger/Logger.h"
#include "data_spaces.h"
#include "office_utils.h"
#include "stream_constants.h"

using namespace rmscore::platform::logger;
using namespace rmscore::common;

namespace{
const char kDrmContent[] = "\11DRMContent";
} // namespace

namespace rmscore {
namespace officeprotector {

MsoOfficeProtector::MsoOfficeProtector(
    const std::string& fileName,
    std::shared_ptr<std::istream> inputStream)
  : mFileName(fileName),
    mInputStream(inputStream) {
  gsf_init();
}

MsoOfficeProtector::~MsoOfficeProtector() {
}

void MsoOfficeProtector::ProtectWithTemplate(
    const UserContext& userContext,
    const ProtectWithTemplateOptions& options,
    std::shared_ptr<std::ostream> outputStream,
    std::shared_ptr<std::atomic<bool>> cancelState) {
  Logger::Hidden("+MsoOfficeProtector::ProtectWithTemplate");
  if (!outputStream->good()) {
    Logger::Error("Output stream invalid");
    throw exceptions::RMSStreamException("Output stream invalid");
  }

  if (IsProtected()) {
    Logger::Error("File is already protected");
    throw exceptions::RMSOfficeFileException(
          "File is already protected",
          exceptions::RMSOfficeFileException::Reason::AlreadyProtected);
  }

  auto inputFileSize = utils::ValidateAndGetFileSize(
        mInputStream.get(),
        utils::MAX_FILE_SIZE_FOR_ENCRYPT);
  auto userPolicyCreationOptions = utils::ConvertToUserPolicyCreationOptions(
        options.allowAuditedExtraction,
        options.cryptoOptions);
  mUserPolicy = modernapi::UserPolicy::CreateFromTemplateDescriptor(
        options.templateDescriptor,
        userContext.userId,
        userContext.authenticationCallback,
        userPolicyCreationOptions,
        options.signedAppData,
        cancelState);
  std::string inputTempFileName = utils::CreateTemporaryFileName(mFileName);
  std::string outputTempFileName = utils::CreateTemporaryFileName("output");
  std::string drmTempFileName = utils::CreateTemporaryFileName("drm");
  std::unique_ptr<utils::TempFileName, utils::TempFile_deleter> inputTempFile(&inputTempFileName);
  std::unique_ptr<utils::TempFileName, utils::TempFile_deleter> outputTempFile(&outputTempFileName);
  std::unique_ptr<utils::TempFileName, utils::TempFile_deleter> drmTempFile(&drmTempFileName);
  try {
    //std::unique_ptr<FILE, FILE_deleter> outputTempFileStream(fopen(outputTempFileName.c_str(), "w+b"));
    ProtectInternal(inputTempFileName, outputTempFileName, drmTempFileName, inputFileSize);
    utils::CopyFromFileToOstream(outputTempFileName, outputStream.get());
  } catch (std::exception&) {
    Logger::Hidden("-MsoOfficeProtector::ProtectWithTemplate");
    throw;
  }
  Logger::Hidden("-MsoOfficeProtector::ProtectWithTemplate");
}

void MsoOfficeProtector::ProtectWithCustomRights(
    const UserContext& userContext,
    const ProtectWithCustomRightsOptions& options,
    std::shared_ptr<std::ostream> outputStream,
    std::shared_ptr<std::atomic<bool>> cancelState) {
  Logger::Hidden("+MsoOfficeProtector::ProtectWithCustomRights");
  if (!outputStream->good()) {
    Logger::Error("Output stream invalid");
    throw exceptions::RMSStreamException("Output stream invalid");
  }

  if (IsProtected()) {
    Logger::Error("File is already protected");
    throw exceptions::RMSOfficeFileException(
          "File is already protected",
          exceptions::RMSOfficeFileException::Reason::AlreadyProtected);
  }
  auto inputFileSize = utils::ValidateAndGetFileSize(
        mInputStream.get(),
        utils::MAX_FILE_SIZE_FOR_ENCRYPT);
  auto userPolicyCreationOptions = utils::ConvertToUserPolicyCreationOptions(
        options.allowAuditedExtraction,
        options.cryptoOptions);
  mUserPolicy = modernapi::UserPolicy::Create(
        const_cast<modernapi::PolicyDescriptor&>(options.policyDescriptor),
        userContext.userId,
        userContext.authenticationCallback,
        userPolicyCreationOptions,
        cancelState);
  std::string inputTempFileName = utils::CreateTemporaryFileName(mFileName);
  std::string outputTempFileName = utils::CreateTemporaryFileName("output");
  std::string drmTempFileName = utils::CreateTemporaryFileName("drm");
  std::unique_ptr<utils::TempFileName, utils::TempFile_deleter> inputTempFile(&inputTempFileName);
  std::unique_ptr<utils::TempFileName, utils::TempFile_deleter> outputTempFile(&outputTempFileName);
  std::unique_ptr<utils::TempFileName, utils::TempFile_deleter> drmTempFile(&drmTempFileName);
  try {
    //std::unique_ptr<FILE, FILE_deleter> outputTempFileStream(fopen(outputTempFileName.c_str(), "w+b"));
    ProtectInternal(inputTempFileName, outputTempFileName, drmTempFileName, inputFileSize);
    utils::CopyFromFileToOstream(outputTempFileName, outputStream.get());
  } catch (std::exception&) {
    Logger::Hidden("-MsoOfficeProtector::ProtectWithCustomRights");
    throw;
  }
  Logger::Hidden("-MsoOfficeProtector::ProtectWithCustomRights");
}

UnprotectResult MsoOfficeProtector::Unprotect(
    const UserContext& userContext,
    const UnprotectOptions& options,
    std::shared_ptr<std::ostream> outputStream,
    std::shared_ptr<std::atomic<bool>> cancelState) {
  Logger::Hidden("+MsoOfficeProtector::UnProtect");
  if (!outputStream->good()) {
    Logger::Error("Output stream invalid");
    throw exceptions::RMSStreamException("Output stream invalid");
  }
  auto inputFileSize = utils::ValidateAndGetFileSize(
        mInputStream.get(),
        utils::MAX_FILE_SIZE_FOR_DECRYPT);
  auto result = UnprotectResult::NORIGHTS;
  std::string inputTempFileName = utils::CreateTemporaryFileName(mFileName);
  std::string outputTempFileName = utils::CreateTemporaryFileName("output");
  std::string drmTempFileName = utils::CreateTemporaryFileName("drm");
  std::unique_ptr<utils::TempFileName, utils::TempFile_deleter> inputTempFile(&inputTempFileName);
  std::unique_ptr<utils::TempFileName, utils::TempFile_deleter> outputTempFile(&outputTempFileName);
  std::unique_ptr<utils::TempFileName, utils::TempFile_deleter> drmTempFile(&drmTempFileName);
  try {
    //std::unique_ptr<FILE, FILE_deleter> outputTempFileStream(fopen(outputTempFileName.c_str(), "w+b"));
    result = UnprotectInternal(
          userContext,
          options,
          inputTempFileName,
          outputTempFileName,
          drmTempFileName,
          inputFileSize,
          cancelState);
    utils::CopyFromFileToOstream(outputTempFileName, outputStream.get());
  } catch (std::exception&) {
    Logger::Hidden("-MetroOfficeProtector::UnProtect");
    throw;
  }
  Logger::Hidden("-MetroOfficeProtector::UnProtect");
  return result;
}

bool MsoOfficeProtector::IsProtected() const {
  Logger::Hidden("+MsoOfficeProtector::IsProtected");
  auto inputFileSize = utils::ValidateAndGetFileSize(
        mInputStream.get(),
        utils::MAX_FILE_SIZE_FOR_DECRYPT);
  std::string inputTempFileName = utils::CreateTemporaryFileName(mFileName);
  std::unique_ptr<utils::TempFileName, utils::TempFile_deleter> inputTempFile(&inputTempFileName);
  bool isProtected = IsProtectedInternal(inputTempFileName, inputFileSize);
  Logger::Hidden("-MsoOfficeProtector::IsProtected");
  return isProtected;
}

void MsoOfficeProtector::ProtectInternal(
    const std::string& inputTempFileName,
    const std::string& outputTempFileName,
    const std::string& drmTempFileName,
    uint64_t inputFileSize) {
  if (mUserPolicy.get() == nullptr) {
    Logger::Error("User Policy creation failed");
    throw exceptions::RMSInvalidArgumentException("User Policy creation failed.");
  }
  utils::CopyFromIstreamToFile(mInputStream.get(), inputTempFileName, inputFileSize);
  std::unique_ptr<GsfInfile, officeprotector::GsfInfile_deleter> inputStg;
  try {
    std::unique_ptr<GsfInput, officeprotector::GsfInput_deleter> gsfInputStdIO(
          gsf_input_stdio_new(inputTempFileName.c_str(), nullptr));
    inputStg.reset(gsf_infile_msole_new(gsfInputStdIO.get(), nullptr));
  } catch (std::exception&) {
    Logger::Hidden("Failed to open file as a valid CFB");
    throw exceptions::RMSOfficeFileException(
          "The file is invalid",
          exceptions::RMSOfficeFileException::Reason::NotOfficeFile);
  }
  auto num_children = gsf_infile_num_children(inputStg.get());
  if (num_children < 0) {
    Logger::Hidden("Empty storage. Nothing to protect");
    throw exceptions::RMSOfficeFileException(
          "The file is invalid",
          exceptions::RMSOfficeFileException::Reason::NotOfficeFile);
  }
  std::unique_ptr<GsfOutput, officeprotector::GsfOutput_deleter> gsfOutputStdIO(
        gsf_output_stdio_new(outputTempFileName.c_str(), nullptr));
  std::unique_ptr<GsfOutfile, officeprotector::GsfOutfile_deleter> outputStg(
        gsf_outfile_msole_new(gsfOutputStdIO.get()));
  {
    std::unique_ptr<GsfOutput, officeprotector::GsfOutput_deleter> gsfDrmStdIO(
          gsf_output_stdio_new(drmTempFileName.c_str(), nullptr));

    std::unique_ptr<GsfOutfile, officeprotector::GsfOutfile_deleter> drmStg(
          gsf_outfile_msole_new(gsfDrmStdIO.get()));
    auto storageElementsList = GetStorageElementsList();
    auto identifierMap = GetIdentifierMap();
    for (int i = 0; i < num_children; i++)
    {
      std::unique_ptr<GsfInput, officeprotector::GsfInput_deleter> inputChild(
            gsf_infile_child_by_index(inputStg.get(), i));
      std::string childName = gsf_input_name(inputChild.get());
      auto num_children_child = gsf_infile_num_children(GSF_INFILE(inputChild.get()));
      if (identifierMap.find(childName) != identifierMap.end()) {
        CopyTemplate(outputStg.get(), identifierMap[childName]);
      }

      if (std::find(storageElementsList.begin(), storageElementsList.end(), childName) !=
          storageElementsList.end()) {
        std::unique_ptr<GsfOutput, officeprotector::GsfOutput_deleter> outputChild(
              gsf_outfile_new_child(outputStg.get(), childName.c_str(), num_children_child > 0));
        auto result = CopyStorage(GSF_INFILE(inputChild.get()), GSF_OUTFILE(outputChild.get()));
      } else {
        std::unique_ptr<GsfOutput, officeprotector::GsfOutput_deleter> drmChild(
              gsf_outfile_new_child(drmStg.get(), childName.c_str(), num_children_child > 0));
        auto result = CopyStorage(GSF_INFILE(inputChild.get()), GSF_OUTFILE(drmChild.get()));
      }
    }
  }
  {
    //Write Dataspaces
    auto dataSpaces = std::make_shared<officeprotector::DataSpaces>(
          false,
          mUserPolicy->DoesUseDeprecatedAlgorithms());
    auto publishingLicense = mUserPolicy->SerializedPolicy();
    dataSpaces->WriteDataSpaces(outputStg.get(), publishingLicense);
  }
  {
    std::unique_ptr<GsfOutput, officeprotector::GsfOutput_deleter> drmEncryptedStream(
          gsf_outfile_new_child(outputStg.get(), kDrmContent, false));
    std::unique_ptr<FILE, utils::FILE_deleter> drmTempFileStream(fopen(drmTempFileName.c_str(), "r+b"));
    auto drmContentSize = utils::ValidateAndGetFileSize(
          drmTempFileStream.get(),
          utils::MAX_FILE_SIZE_FOR_ENCRYPT);
    officeutils::WriteStreamHeader(drmEncryptedStream.get(), drmContentSize);
    EncryptStream(drmTempFileStream.get(), drmEncryptedStream.get(), drmContentSize);
  }
}

UnprotectResult MsoOfficeProtector::UnprotectInternal(
    const UserContext& userContext,
    const UnprotectOptions& options,
    const std::string& inputTempFileName,
    const std::string& outputTempFileName,
    const std::string& drmTempFileName,
    uint64_t inputFileSize,
    std::shared_ptr<std::atomic<bool>> cancelState) {
  utils::CopyFromIstreamToFile(mInputStream.get(), inputTempFileName, inputFileSize);
  std::unique_ptr<GsfInfile, officeprotector::GsfInfile_deleter> inputStg;
  try {
    std::unique_ptr<GsfInput, officeprotector::GsfInput_deleter> gsfInputStdIO(
          gsf_input_stdio_new(inputTempFileName.c_str(), nullptr));
    inputStg.reset(gsf_infile_msole_new(gsfInputStdIO.get(), nullptr));
  } catch (std::exception&) {
    Logger::Hidden("Failed to open file as a valid CFB");
    throw exceptions::RMSOfficeFileException(
          "The file is invalid",
          exceptions::RMSOfficeFileException::Reason::NotOfficeFile);
  }
  ByteArray publishingLicense;
  try {
    auto dataSpaces = std::make_shared<officeprotector::DataSpaces>(false, true);
    dataSpaces->ReadDataSpaces(inputStg.get(), publishingLicense);
  } catch (std::exception&) {
    Logger::Hidden("Failed to read dataspaces");
    throw;
  }
  modernapi::PolicyAcquisitionOptions policyAcquisitionOptions = options.offlineOnly?
        modernapi::PolicyAcquisitionOptions::POL_OfflineOnly :
        modernapi::PolicyAcquisitionOptions::POL_None;
  auto cacheMask = modernapi::RESPONSE_CACHE_NOCACHE;
  if (options.useCache) {
    cacheMask = static_cast<modernapi::ResponseCacheFlags>(
          modernapi::RESPONSE_CACHE_INMEMORY|
          modernapi::RESPONSE_CACHE_ONDISK |
          modernapi::RESPONSE_CACHE_CRYPTED);
  }
  auto policyRequest = modernapi::UserPolicy::Acquire(
        publishingLicense,
        userContext.userId,
        userContext.authenticationCallback,
        &userContext.consentCallback,
        policyAcquisitionOptions,
        cacheMask,
        cancelState);
  if (policyRequest->Status != modernapi::GetUserPolicyResultStatus::Success) {
    Logger::Error("UserPolicy::Acquire unsuccessful", policyRequest->Status);
    throw exceptions::RMSOfficeFileException(
          "The file has been corrupted",
          exceptions::RMSOfficeFileException::Reason::CorruptFile);
  }
  mUserPolicy = policyRequest->Policy;
  if (mUserPolicy.get() == nullptr) {
    Logger::Error("User Policy acquisition failed");
    throw exceptions::RMSInvalidArgumentException("User Policy acquisition failed.");
  }

  if (!mUserPolicy->DoesUseDeprecatedAlgorithms()) {
    throw exceptions::RMSLogicException(exceptions::RMSException::ErrorTypes::NotSupported,
                                        "CBC Decryption with Office files is not yet supported");
  }
  std::unique_ptr<GsfOutput, officeprotector::GsfOutput_deleter> gsfOutputStdIO(
        gsf_output_stdio_new(outputTempFileName.c_str(), nullptr));
  std::unique_ptr<GsfOutfile, officeprotector::GsfOutfile_deleter> outputStg(
        gsf_outfile_msole_new(gsfOutputStdIO.get()));
  {
    std::unique_ptr<FILE, utils::FILE_deleter> drmTempFileStream(fopen(drmTempFileName.c_str(), "w+b"));
    std::unique_ptr<GsfInput, officeprotector::GsfInput_deleter> drmEncryptedStream(
          gsf_infile_child_by_name(inputStg.get(), kDrmContent));
    if (drmEncryptedStream.get() == nullptr) {
      Logger::Error("Encrypted data not found");
      throw exceptions::RMSOfficeFileException(
            "The file has been corrupted",
            exceptions::RMSOfficeFileException::Reason::CorruptFile);
    }
    uint64_t originalFileSize = 0;
    officeutils::ReadStreamHeader(drmEncryptedStream.get(), originalFileSize);
    DecryptStream(drmTempFileStream.get(), drmEncryptedStream.get(), originalFileSize);
    auto num_children = gsf_infile_num_children(inputStg.get());
    auto storageElementsList = GetStorageElementsList();
    for (int i = 0; i < num_children; i++) {
      std::unique_ptr<GsfInput, officeprotector::GsfInput_deleter> inputChild(
            gsf_infile_child_by_index(inputStg.get(), i));
      std::string childName = gsf_input_name(inputChild.get());
      if (std::find(storageElementsList.begin(), storageElementsList.end(), childName) !=
          storageElementsList.end()) {
        auto num_children_child = gsf_infile_num_children(GSF_INFILE(inputChild.get()));
        std::unique_ptr<GsfOutput, officeprotector::GsfOutput_deleter> outputChild(
              gsf_outfile_new_child(outputStg.get(), childName.c_str(),
                                    num_children_child > 0));
        auto result = CopyStorage(GSF_INFILE(inputChild.get()), GSF_OUTFILE(outputChild.get()));
      }
    }
  }
  std::unique_ptr<GsfInput, officeprotector::GsfInput_deleter> gsfDrmStdIO(
        gsf_input_stdio_new(drmTempFileName.c_str(), nullptr));
  std::unique_ptr<GsfInfile, officeprotector::GsfInfile_deleter> drmStg(
        gsf_infile_msole_new(gsfDrmStdIO.get(), nullptr));
  CopyStorage(drmStg.get(), outputStg.get());
  return (UnprotectResult)policyRequest->Status;
}

bool MsoOfficeProtector::IsProtectedInternal(
    const std::string& inputTempFileName,
    uint64_t inputFileSize) const {
  utils::CopyFromIstreamToFile(mInputStream.get(), inputTempFileName, inputFileSize);
  try {
    std::unique_ptr<GsfInput, officeprotector::GsfInput_deleter> gsfInputStdIO(
          gsf_input_stdio_new(inputTempFileName.c_str(), nullptr));
    std::unique_ptr<GsfInfile, officeprotector::GsfInfile_deleter> stg(
          gsf_infile_msole_new(gsfInputStdIO.get(), nullptr));
    auto dataSpaces = std::make_shared<officeprotector::DataSpaces>(false);
    ByteArray publishingLicense;
    dataSpaces->ReadDataSpaces(stg.get(), publishingLicense);
  } catch (std::exception& e) {
    if (static_cast<exceptions::RMSException&>(e).error() ==
        static_cast<int>(exceptions::RMSException::ErrorTypes::OfficeFileError) &&
        (static_cast<exceptions::RMSOfficeFileException&>(e).reason() ==
         exceptions::RMSOfficeFileException::Reason::NonRMSProtected)) {
      return true;
    } else {
      return false;
    }
  }
  return true;
}

std::shared_ptr<rmscrypto::api::BlockBasedProtectedStream> MsoOfficeProtector::CreateProtectedStream(
    const rmscrypto::api::SharedStream& stream,
    uint64_t streamSize,
    std::shared_ptr<rmscrypto::api::ICryptoProvider> cryptoProvider) {
  // Cache block size to be 512 for cbc512, 4096 for cbc4k and ecb
  uint64_t protectedStreamBlockSize = cryptoProvider->GetBlockSize() == 512 ? 512 : 4096;
  return rmscrypto::api::BlockBasedProtectedStream::Create(
        cryptoProvider,
        stream,
        0,
        streamSize,
        protectedStreamBlockSize);
}

void MsoOfficeProtector::EncryptStream(
    FILE* drmStream,
    GsfOutput* drmEncryptedStream,
    uint64_t inputFileSize) {
  auto cryptoProvider = mUserPolicy->GetImpl()->GetCryptoProvider();
  mBlockSize = cryptoProvider->GetBlockSize();
  std::vector<uint8_t> buffer(utils::BUF_SIZE_BYTES);
  uint64_t readPosition  = 0;
  bool isECB = mUserPolicy->DoesUseDeprecatedAlgorithms();
  uint64_t totalSize = isECB? ((inputFileSize + mBlockSize - 1) & ~(mBlockSize - 1)) :
                              inputFileSize;
  while (totalSize - readPosition > 0) {
    uint64_t offsetRead  = readPosition;
    uint64_t toProcess   = std::min(utils::BUF_SIZE_BYTES, totalSize - readPosition);
    readPosition  += toProcess;

    auto sstream = std::make_shared<std::stringstream>();
    std::shared_ptr<std::iostream> iosstream = sstream;
    auto sharedStringStream = rmscrypto::api::CreateStreamFromStdStream(iosstream);
    auto pStream = CreateProtectedStream(sharedStringStream, 0, cryptoProvider);

    fseek(drmStream, offsetRead, SEEK_SET);
    fread(&buffer[0], toProcess, 1, drmStream);
    pStream->WriteAsync(buffer.data(), toProcess, 0, std::launch::deferred).get();
    pStream->FlushAsync(std::launch::deferred).get();
    std::string encryptedData = sstream->str();
    auto encryptedDataLen = encryptedData.length();
    gsf_output_write(
          drmEncryptedStream,
          encryptedDataLen,
          reinterpret_cast<const uint8_t*>(encryptedData.data()));
  }
}

void MsoOfficeProtector::DecryptStream(
    FILE* drmStream,
    GsfInput* drmEncryptedStream,
    uint64_t originalFileSize) {
  auto cryptoProvider = mUserPolicy->GetImpl()->GetCryptoProvider();
  mBlockSize = cryptoProvider->GetBlockSize();
  std::vector<uint8_t> buffer(utils::BUF_SIZE_BYTES);
  uint64_t readPosition  = 0;
  uint64_t writePosition = 0;
  uint64_t totalSize = (uint64_t)gsf_input_size(drmEncryptedStream) - sizeof(uint64_t);
  while (totalSize - readPosition > 0) {
    uint64_t offsetWrite = writePosition;
    uint64_t toProcess   = std::min(utils::BUF_SIZE_BYTES, totalSize - readPosition);
    uint64_t originalRemaining = std::min(utils::BUF_SIZE_BYTES, originalFileSize - readPosition);
    readPosition  += toProcess;
    writePosition += toProcess;

    gsf_input_read(drmEncryptedStream, toProcess, &buffer[0]);
    std::string encryptedData(reinterpret_cast<const char *>(buffer.data()), toProcess);
    auto sstream = std::make_shared<std::stringstream>();
    sstream->str(encryptedData);
    std::shared_ptr<std::iostream> iosstream = sstream;
    auto sharedStringStream = rmscrypto::api::CreateStreamFromStdStream(iosstream);
    auto pStream = CreateProtectedStream(sharedStringStream, encryptedData.length(), cryptoProvider);
    pStream->ReadAsync(&buffer[0], toProcess, 0, std::launch::deferred).get();

    fseek(drmStream, offsetWrite, SEEK_SET);
    fwrite(reinterpret_cast<const char *>(buffer.data()), toProcess, 1, drmStream );
  }
  fflush(drmStream);
}

bool MsoOfficeProtector::CopyStorage(GsfInfile *src, GsfOutfile *dest) {
  if (src == nullptr || dest == nullptr) {
    return false;
  }
  auto childCount = gsf_infile_num_children(src);
  if (childCount == -1) {
    //It's a stream
    return CopyStream(GSF_INPUT(src), GSF_OUTPUT(dest));
  }
  bool result = true;
  for (int i = 0; i < childCount; i++) {
    std::unique_ptr<GsfInput, officeprotector::GsfInput_deleter> srcChild(
          gsf_infile_child_by_index(src, i));
    std::string childName = gsf_input_name(srcChild.get());
    if (gsf_infile_num_children(GSF_INFILE(srcChild.get())) < 0) {
      // Child is a stream
      std::unique_ptr<GsfOutput, officeprotector::GsfOutput_deleter> destChild(
            gsf_outfile_new_child(dest, childName.c_str(), false));
      result = result & CopyStream(srcChild.get(), destChild.get());
    } else {
      // Child is a storage
      std::unique_ptr<GsfOutput, officeprotector::GsfOutput_deleter> destChild(
            gsf_outfile_new_child(dest, childName.c_str(), true));
      result = result & CopyStorage(GSF_INFILE(srcChild.get()),
                                    GSF_OUTFILE(destChild.get()));
    }
  }
  return result;
}

bool MsoOfficeProtector::CopyStream(GsfInput *src, GsfOutput *dest) {
  if (src == nullptr || dest == nullptr) {
    return false;
  }
  gsf_input_seek(src, 0, G_SEEK_SET);
  gsf_output_seek(dest, 0, G_SEEK_SET);
  return gsf_input_copy(src, dest);
}

void MsoOfficeProtector::CopyTemplate(GsfOutfile* dest, uint32_t identifier) {
  if (dest == nullptr) {
    return;
  }
  switch (identifier) {
    case 0: {
      {
        std::unique_ptr<GsfOutput, officeprotector::GsfOutput_deleter> tableStream(
              gsf_outfile_new_child(dest, "1Table", false));
        gsf_output_write(tableStream.get(), sizeof(kTable), kTable);
      }
      {
        std::unique_ptr<GsfOutput, officeprotector::GsfOutput_deleter> tableStream(
              gsf_outfile_new_child(dest, "WordDocument", false));
        gsf_output_write(tableStream.get(), sizeof(kWordDocument), kWordDocument);
      }
      break;
    }
    case 1: {
      {
        std::unique_ptr<GsfOutput, officeprotector::GsfOutput_deleter> tableStream(
              gsf_outfile_new_child(dest, "Current User", false));
        gsf_output_write(tableStream.get(), sizeof(kCurrentUser), kCurrentUser);
      }
      {
        std::unique_ptr<GsfOutput, officeprotector::GsfOutput_deleter> tableStream(
              gsf_outfile_new_child(dest, "PowerPoint Document", false));
        gsf_output_write(tableStream.get(), sizeof(kPowerPointDocument), kPowerPointDocument);
      }
      break;
    }
    case 2: {
      {
        std::unique_ptr<GsfOutput, officeprotector::GsfOutput_deleter> tableStream(
              gsf_outfile_new_child(dest, "Workbook", false));
        gsf_output_write(tableStream.get(), sizeof(kWorkbook), kWorkbook);
      }
      break;
    }
    default: {
      break;
    }
  }
}

const std::vector<std::string>& MsoOfficeProtector::GetStorageElementsList() {
  static std::vector<std::string> storageElementsList = {"_signatures", "\1CompObj", "Macros",
                                                         "_VBA_PROJECT_CUR", "\5SummaryInformation",
                                                         "\5DocumentSummaryInformation",
                                                         "MsoDataStore"};
  return storageElementsList;
}

const std::map<std::string, uint32_t>& MsoOfficeProtector::GetIdentifierMap() {
  static std::map<std::string, uint32_t> identifierMap = {{"WordDocument", 0},
                                                          {"PowerPoint Document", 1},
                                                          {"Workbook", 2}};
  return identifierMap;
}

} // namespace officeprotector
} // namespace rmscore