/*
 * ======================================================================
 * Copyright (c) Microsoft Open Technologies, Inc.  All rights reserved.
 * Licensed under the MIT License.
 * See LICENSE.md in the project root for license information.
 * ======================================================================
*/

#ifndef RMS_SDK_OFFICE_PROTECTOR_DATASPACES_H
#define RMS_SDK_OFFICE_PROTECTOR_DATASPACES_H

#include "idata_spaces.h"
#include <memory>
#include <string>
#include <gsf/gsf.h>
#include "../Common/CommonTypes.h"

using namespace rmscore::common;

namespace rmscore {
namespace officeprotector {

/* DataSpaces contains the transform process. It contains the version, license etc
 * For more information, go to https://msdn.microsoft.com/en-us/library/aa767782(v=vs.85).aspx
 */
class DataSpaces : public IDataSpaces {
public:
  DataSpaces(bool isMetro, bool doesUseDeprecatedAlgorithm = true);
  ~DataSpaces();
  void WriteDataSpaces(GsfOutfile* stg, const ByteArray& publishingLicense) override;
  void ReadDataSpaces(GsfInfile* stg, ByteArray& publishingLicense) override;

private:
  void WriteVersion(GsfOutput* stm, const std::string& content);
  void ReadAndVerifyVersion(GsfInput* stm, const std::string& contentExpected);
  void WriteDataSpaceMap(GsfOutput* stm);
  void WriteDrmDataSpace(GsfOutput *stm);
  void WriteTxInfo(GsfOutput* stm, const std::string& txClassName, const std::string& featureName);
  void ReadTxInfo(GsfInput* stm, const std::string& txClassName, const std::string& featureName);
  void WritePrimary(GsfOutput* stm,  const ByteArray& publishingLicense);
  void ReadPrimary(GsfInput* stm, ByteArray& publishingLicense);
  bool mIsMetro = true;
  bool mDoesUseDeprecatedAlgorithm;
};

struct GsfOutput_deleter {
  void operator () (GsfOutput* obj) const {
    if (!gsf_output_is_closed(obj)) {
      gsf_output_close(obj);
    }
    g_object_unref(G_OBJECT(obj));
  }
};

struct GsfOutfile_deleter {
  void operator () (GsfOutfile* obj) const {
    if (!gsf_output_is_closed(GSF_OUTPUT(obj))) {
      gsf_output_close(GSF_OUTPUT(obj));
    }
    g_object_unref(G_OBJECT(obj));
  }
};

struct GsfInput_deleter {
  void operator () (GsfInput* obj) const {
    g_object_unref(G_OBJECT(obj));
  }
};

struct GsfInfile_deleter {
  void operator () (GsfInfile* obj) const {
    g_object_unref(G_OBJECT(obj));
  }
};

} // namespace officeprotector
} // namespace rmscore

#endif // RMS_SDK_OFFICE_PROTECTOR_DATASPACES_H
