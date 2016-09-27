
/**
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * Copyright (c) 2016 Tino Reichardt
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "lz4frame.h"
#include "lz4mt.h"

/* will be used for lib errors */
size_t lz4mt_errcode;

/* ****************************************
 * LZ4MT Error Management
 ******************************************/

/**
 * LZ4MT_isError() - tells if a return value is an error code
 */
unsigned LZ4MT_isError(size_t code)
{
#if 0
	printf("no_error            = %zu\n", ERROR(no_error));
	printf("memory_allocation   = %zu\n", ERROR(memory_allocation));
	printf("compression_library = %zu\n", ERROR(compression_library));
	printf("maxCode             = %zu\n", ERROR(maxCode));
	printf("result              = %zu\n", code);
	printf("ok ZSTD_decompressStream() result=%zu >=%d max=%zu\n", code, code > ERROR(maxCode), ERROR(maxCode));
#endif
	return (code > ERROR(maxCode));
}

/**
 * LZ4MT_getErrorString() - give error code string from function result
 */
const char *LZ4MT_getErrorString(size_t code)
{
	if (LZ4F_isError(lz4mt_errcode))
		return LZ4F_getErrorName(lz4mt_errcode);

	static const char *notErrorCode = "Unspecified error lz4mt code";
	switch ((LZ4MT_ErrorCode)(0-code)) {
	case PREFIX(no_error):
		return "No error detected";
	case PREFIX(memory_allocation):
		return "Allocation error : not enough memory";
	case PREFIX(read_fail):
		return "Read failure";
	case PREFIX(write_fail):
		return "Write failure";
	case PREFIX(data_error):
		return "Malformed input";
	case PREFIX(frame_compress):
		return "Could not compress frame at once";
	case PREFIX(frame_decompress):
		return "Could not decompress frame at once";
	case PREFIX(compressionParameter_unsupported):
		return "Compression parameter is out of bound";
	case PREFIX(compression_library):
		return "Compression library reports failure";
	case PREFIX(maxCode):
	default:
		return notErrorCode;
	}
}
