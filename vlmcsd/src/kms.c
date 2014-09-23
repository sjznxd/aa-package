#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>
#if !defined(_WIN32)
#include <sys/socket.h>
#endif
#include "output.h"
#include "crypto.h"
#include "endian.h"

WCHAR utf8_to_ucs2_char (const unsigned char * input, const unsigned char ** end_ptr)
{
    *end_ptr = input;
    if (input[0] == 0)
        return (WCHAR)-1;

    if (input[0] < 0x80) {
        *end_ptr = input + 1;
        return LE16(input[0]);
    }

    if ((input[0] & 0xE0) == 0xE0) {

    	if (input[1] == 0 || input[2] == 0)
            return (WCHAR)-1;

    	*end_ptr = input + 3;

    	return
            LE16((input[0] & 0x0F)<<12 |
            (input[1] & 0x3F)<<6  |
            (input[2] & 0x3F));
    }

    if ((input[0] & 0xC0) == 0xC0) {
        if (input[1] == 0)
            return (WCHAR)-1;

        *end_ptr = input + 2;

        return
            LE16((input[0] & 0x1F)<<6  |
            (input[1] & 0x3F));
    }
    return (WCHAR)-1;
}

static int ucs2_to_utf8_char (const WCHAR ucs2_le, char * utf8)
{
    const WCHAR ucs2 = LE16(ucs2_le);

    if (ucs2 < 0x80) {
        utf8[0] = ucs2;
        utf8[1] = '\0';
        return 1;
    }

    if (ucs2 >= 0x80  && ucs2 < 0x800) {
        utf8[0] = (ucs2 >> 6)   | 0xC0;
        utf8[1] = (ucs2 & 0x3F) | 0x80;
        utf8[2] = '\0';
        return 2;
    }

    if (ucs2 >= 0x800 && ucs2 < 0xFFFF) {

    	if (ucs2 >= 0xD800 && ucs2 <= 0xDFFF) {
    		/* Ill-formed (UTF-16 ouside of BMP) */
    		return -1;
    	}

    	utf8[0] = ((ucs2 >> 12)       ) | 0xE0;
        utf8[1] = ((ucs2 >> 6 ) & 0x3F) | 0x80;
        utf8[2] = ((ucs2      ) & 0x3F) | 0x80;
        utf8[3] = '\0';
        return 3;
    }

    return -1;
}

size_t utf8_to_ucs2(WCHAR* const ucs2_le, const char* const utf8, const size_t maxucs2, const size_t maxutf8)
{
	const unsigned char* current_utf8 = (unsigned char*)utf8;
	WCHAR* current_ucs2_le = ucs2_le;

	for (; *current_utf8; current_ucs2_le++)
	{
		size_t size = (char*)current_utf8 - utf8;

		if (size >= maxutf8) return (size_t)-1;
		if (((*current_utf8 & 0xc0) == 0xc0) && (size >= maxutf8 - 1)) return (size_t)-1;
		if (((*current_utf8 & 0xe0) == 0xe0) && (size >= maxutf8 - 2)) return (size_t)-1;
		if (current_ucs2_le - ucs2_le >= (intptr_t)maxucs2 - 1) return (size_t)-1;

		*current_ucs2_le = utf8_to_ucs2_char(current_utf8, &current_utf8);
		current_ucs2_le[1] = 0;

		if (*current_ucs2_le == (WCHAR)-1) return (size_t)-1;
	}
	return current_ucs2_le - ucs2_le;
}

BOOL ucs2_to_utf8(const WCHAR* const ucs2_le, char* utf8, size_t maxucs2, size_t maxutf8)
{
	char utf8_char[4];
	const WCHAR* current_ucs2 = ucs2_le;
	unsigned int index_utf8 = 0;

	for(*utf8 = 0; *current_ucs2; current_ucs2++)
	{
		if (current_ucs2 - ucs2_le > (intptr_t)maxucs2) return 0;
		int len = ucs2_to_utf8_char(*current_ucs2, utf8_char);
		if (index_utf8 + len > maxutf8) return 0;
		strncat(utf8, utf8_char, len);
		index_utf8+=len;
	}

	return !0;
}


// Unix time is seconds from 1970-01-01. Should be 64 bits to avoid Year 2035 overflow bug.
// FILETIME is 100 nanoseconds from 1601-01-01. Must be 64 bits.
void GetUnixTimeAsFileTime(FILETIME *const ts)
{
	int64_t unixtime = (int64_t)time(NULL);
	int64_t *filetime = (int64_t*)ts;

	*filetime = LE64( (unixtime + 11644473600LL) * 10000000LL );
}

__pure int64_t FileTimeToUnixTime(const FILETIME *const ts)
{
	return LE64( *((const int64_t *const)ts) ) / 10000000LL - 11644473600LL;
}


#define FRIENDLY_NAME_WINDOWS "Windows"
#define FRIENDLY_NAME_OFFICE2010 "Office2010"
#define FRIENDLY_NAME_OFFICE2013 "Office2013"

#ifndef EPID_WINDOWS
#define EPID_WINDOWS "06401-00206-271-295042-03-1033-9600.0000-2542014"
#endif

#ifndef EPID_OFFICE2010
#define EPID_OFFICE2010 "05426-00096-199-299483-03-1033-9600.0000-2542014"
#endif

#ifndef EPID_OFFICE2013
#define EPID_OFFICE2013 "55041-00206-234-398302-03-1033-9600.0000-2542014"
#endif

#ifndef HWID
#define HWID 0x36, 0x4F, 0x46, 0x3A, 0x88, 0x63, 0xD3, 0x5F
#endif

#ifndef NO_BASIC_PRODUCT_LIST
// Do not change the order of this list. Append items as necessary
const KmsIdList ProductList[] = {
	/* 000 */ { { 0x212a64dc, 0x43b1, 0x4d3d, { 0xa3, 0x0c, 0x2f, 0xc6, 0x9d, 0x20, 0x95, 0xc6 } } /*"212a64dc-43b1-4d3d-a30c-2fc69d2095c6"*/, "Vista",                    EPID_WINDOWS,    4, 25 },
	/* 001 */ { { 0x7fde5219, 0xfbfa, 0x484a, { 0x82, 0xc9, 0x34, 0xd1, 0xad, 0x53, 0xe8, 0x56 } } /*"7fde5219-fbfa-484a-82c9-34d1ad53e856"*/, "Windows 7",                EPID_WINDOWS,    4, 25 },
	/* 002 */ { { 0x3c40b358, 0x5948, 0x45af, { 0x92, 0x3b, 0x53, 0xd2, 0x1f, 0xcc, 0x7e, 0x79 } } /*"3c40b358-5948-45af-923b-53d21fcc7e79"*/, "Windows 8",                EPID_WINDOWS,    5, 25 },
	/* 003 */ { { 0x5f94a0bb, 0xd5a0, 0x4081, { 0xa6, 0x85, 0x58, 0x19, 0x41, 0x8b, 0x2f, 0xe0 } } /*"5f94a0bb-d5a0-4081-a685-5819418b2fe0"*/, "Windows 8.x Beta/Preview", EPID_WINDOWS,    5, 25 },
	/* 004 */ { { 0xbbb97b3b, 0x8ca4, 0x4a28, { 0x97, 0x17, 0x89, 0xfa, 0xbd, 0x42, 0xc4, 0xac } } /*"bbb97b3b-8ca4-4a28-9717-89fabd42c4ac"*/, "Windows 8 Non-VL",         EPID_WINDOWS,    5, 25 },
	/* 005 */ { { 0xcb8fc780, 0x2c05, 0x495a, { 0x97, 0x10, 0x85, 0xaf, 0xff, 0xc9, 0x04, 0xd7 } } /*"cb8fc780-2c05-495a-9710-85afffc904d7"*/, "Windows 8.1",              EPID_WINDOWS,    6, 25 },
	/* 006 */ { { 0x6d646890, 0x3606, 0x461a, { 0x86, 0xab, 0x59, 0x8b, 0xb8, 0x4a, 0xce, 0x82 } } /*"6d646890-3606-461a-86ab-598bb84ace82"*/, "Windows 8.1 Non-VL",       EPID_WINDOWS,    6, 25 },
	/* 007 */ { { 0x33e156e4, 0xb76f, 0x4a52, { 0x9f, 0x91, 0xf6, 0x41, 0xdd, 0x95, 0xac, 0x48 } } /*"33e156e4-b76f-4a52-9f91-f641dd95ac48"*/, "Windows 2008 A",           EPID_WINDOWS,    4,  5 },
	/* 008 */ { { 0x8fe53387, 0x3087, 0x4447, { 0x89, 0x85, 0xf7, 0x51, 0x32, 0x21, 0x5a, 0xc9 } } /*"8fe53387-3087-4447-8985-f75132215ac9"*/, "Windows 2008 B",           EPID_WINDOWS,    4,  5 },
	/* 009 */ { { 0x8a21fdf3, 0xcbc5, 0x44eb, { 0x83, 0xf3, 0xfe, 0x28, 0x4e, 0x66, 0x80, 0xa7 } } /*"8a21fdf3-cbc5-44eb-83f3-fe284e6680a7"*/, "Windows 2008 C",           EPID_WINDOWS,    4,  5 },
	/* 010 */ { { 0x0fc6ccaf, 0xff0e, 0x4fae, { 0x9d, 0x08, 0x43, 0x70, 0x78, 0x5b, 0xf7, 0xed } } /*"0fc6ccaf-ff0e-4fae-9d08-4370785bf7ed"*/, "Windows 2008 R2 A",        EPID_WINDOWS,    4,  5 },
	/* 011 */ { { 0xca87f5b6, 0xcd46, 0x40c0, { 0xb0, 0x6d, 0x8e, 0xcd, 0x57, 0xa4, 0x37, 0x3f } } /*"ca87f5b6-cd46-40c0-b06d-8ecd57a4373f"*/, "Windows 2008 R2 B",        EPID_WINDOWS,    4,  5 },
	/* 012 */ { { 0xb2ca2689, 0xa9a8, 0x42d7, { 0x93, 0x8d, 0xcf, 0x8e, 0x9f, 0x20, 0x19, 0x58 } } /*"b2ca2689-a9a8-42d7-938d-cf8e9f201958"*/, "Windows 2008 R2 C",        EPID_WINDOWS,    4,  5 },
	/* 013 */ { { 0x8665cb71, 0x468c, 0x4aa3, { 0xa3, 0x37, 0xcb, 0x9b, 0xc9, 0xd5, 0xea, 0xac } } /*"8665cb71-468c-4aa3-a337-cb9bc9d5eaac"*/, "Windows 2012",             EPID_WINDOWS,    5,  5 },
	/* 014 */ { { 0x8456EFD3, 0x0C04, 0x4089, { 0x87, 0x40, 0x5b, 0x72, 0x38, 0x53, 0x5a, 0x65 } } /*"8456EFD3-0C04-4089-8740-5B7238535A65"*/, "Windows 2012 R2",          EPID_WINDOWS,    6,  5 },
	/* 015 */ { { 0xe85af946, 0x2e25, 0x47b7, { 0x83, 0xe1, 0xbe, 0xbc, 0xeb, 0xea, 0xc6, 0x11 } } /*"e85af946-2e25-47b7-83e1-bebcebeac611"*/, "Office 2010",              EPID_OFFICE2010, 4,  5 },
	/* 016 */ { { 0xe6a6f1bf, 0x9d40, 0x40c3, { 0xaa, 0x9f, 0xc7, 0x7b, 0xa2, 0x15, 0x78, 0xc0 } } /*"e6a6f1bf-9d40-40c3-aa9f-c77ba21578c0"*/, "Office 2013",              EPID_OFFICE2013, 6,  5 },
	/* 017 */ { { 0x00000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }, NULL, NULL, 0, 0 }
};
#endif

// Application ID is used by KMS server to count KeyManagementServiceCurrentCount
// Do not change the order of this list. Append items as necessary
const KmsIdList AppList[] = {
	/* 000 */ { { 0x55c92734, 0xd682, 0x4d71, { 0x98, 0x3e, 0xd6, 0xec, 0x3f, 0x16, 0x05, 0x9f } } /*"55C92734-D682-4D71-983E-D6EC3F16059F"*/, FRIENDLY_NAME_WINDOWS,    EPID_WINDOWS },
	/* 001 */ { { 0x59A52881, 0xa989, 0x479d, { 0xaf, 0x46, 0xf2, 0x75, 0xc6, 0x37, 0x06, 0x63 } } /*"59A52881-A989-479D-AF46-F275C6370663"*/, FRIENDLY_NAME_OFFICE2010, EPID_OFFICE2010 },
	/* 002 */ { { 0x0FF1CE15, 0xA989, 0x479D, { 0xaf, 0x46, 0xf2, 0x75, 0xc6, 0x37, 0x06, 0x63 } } /*"0FF1CE15-A989-479D-AF46-F275C6370663"*/, FRIENDLY_NAME_OFFICE2013, EPID_OFFICE2013 },
	/* 003 */ { { 0x00000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }, NULL, NULL, 0, 0 }
};

#ifndef NO_EXTENDED_PRODUCT_LIST
const KmsIdList ExtendedProductList [] = {
	/* 000 */ { { 0xad2542d4, 0x9154, 0x4c6d, { 0x8a, 0x44, 0x30, 0xf1, 0x1e, 0xe9, 0x69, 0x89, } } /*ad2542d4-9154-4c6d-8a44-30f11ee96989*/, "Windows Server 2008 Standard",            EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN2008A },
	/* 001 */ { { 0x2401e3d0, 0xc50a, 0x4b58, { 0x87, 0xb2, 0x7e, 0x79, 0x4b, 0x7d, 0x26, 0x07, } } /*2401e3d0-c50a-4b58-87b2-7e794b7d2607*/, "Windows Server 2008 Standard V",          EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN2008A },
	/* 002 */ { { 0x68b6e220, 0xcf09, 0x466b, { 0x92, 0xd3, 0x45, 0xcd, 0x96, 0x4b, 0x95, 0x09, } } /*68b6e220-cf09-466b-92d3-45cd964b9509*/, "Windows Server 2008 Datacenter",          EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN2008C },
	/* 003 */ { { 0xfd09ef77, 0x5647, 0x4eff, { 0x80, 0x9c, 0xaf, 0x2b, 0x64, 0x65, 0x9a, 0x45, } } /*fd09ef77-5647-4eff-809c-af2b64659a45*/, "Windows Server 2008 Datacenter V",        EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN2008C },
	/* 004 */ { { 0xc1af4d90, 0xd1bc, 0x44ca, { 0x85, 0xd4, 0x00, 0x3b, 0xa3, 0x3d, 0xb3, 0xb9, } } /*c1af4d90-d1bc-44ca-85d4-003ba33db3b9*/, "Windows Server 2008 Enterprise",          EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN2008B },
	/* 005 */ { { 0x8198490a, 0xadd0, 0x47b2, { 0xb3, 0xba, 0x31, 0x6b, 0x12, 0xd6, 0x47, 0xb4, } } /*8198490a-add0-47b2-b3ba-316b12d647b4*/, "Windows Server 2008 Enterprise V",        EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN2008B  },
	/* 006 */ { { 0xddfa9f7c, 0xf09e, 0x40b9, { 0x8c, 0x1a, 0xbe, 0x87, 0x7a, 0x9a, 0x7f, 0x4b, } } /*ddfa9f7c-f09e-40b9-8c1a-be877a9a7f4b*/, "Windows Server 2008 Web",                 EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN2008A },
	/* 007 */ { { 0x7afb1156, 0x2c1d, 0x40fc, { 0xb2, 0x60, 0xaa, 0xb7, 0x44, 0x2b, 0x62, 0xfe, } } /*7afb1156-2c1d-40fc-b260-aab7442b62fe*/, "Windows Server 2008 Compute Cluster",     EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN2008C },
	/* 008 */ { { 0x68531fb9, 0x5511, 0x4989, { 0x97, 0xbe, 0xd1, 0x1a, 0x0f, 0x55, 0x63, 0x3f, } } /*68531fb9-5511-4989-97be-d11a0f55633f*/, "Windows Server 2008 R2 Standard",         EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN2008R2A },
	/* 009 */ { { 0x7482e61b, 0xc589, 0x4b7f, { 0x8e, 0xcc, 0x46, 0xd4, 0x55, 0xac, 0x3b, 0x87, } } /*7482e61b-c589-4b7f-8ecc-46d455ac3b87*/, "Windows Server 2008 R2 Datacenter",       EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN2008R2C },
	/* 010 */ { { 0x620e2b3d, 0x09e7, 0x42fd, { 0x80, 0x2a, 0x17, 0xa1, 0x36, 0x52, 0xfe, 0x7a, } } /*620e2b3d-09e7-42fd-802a-17a13652fe7a*/, "Windows Server 2008 R2 Enterprise",       EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN2008R2B },
	/* 011 */ { { 0xa78b8bd9, 0x8017, 0x4df5, { 0xb8, 0x6a, 0x09, 0xf7, 0x56, 0xaf, 0xfa, 0x7c, } } /*a78b8bd9-8017-4df5-b86a-09f756affa7c*/, "Windows Server 2008 R2 Web",              EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN2008R2A },
	/* 012 */ { { 0xcda18cf3, 0xc196, 0x46ad, { 0xb2, 0x89, 0x60, 0xc0, 0x72, 0x86, 0x99, 0x94, } } /*cda18cf3-c196-46ad-b289-60c072869994*/, "Windows Server 2008 R2 Compute Cluster",  EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN2008R2C },
	/* 013 */ { { 0xd3643d60, 0x0c42, 0x412d, { 0xa7, 0xd6, 0x52, 0xe6, 0x63, 0x53, 0x27, 0xf6, } } /*d3643d60-0c42-412d-a7d6-52e6635327f6*/, "Windows Server 2012 Datacenter",          EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN2012 },
	/* 014 */ { { 0xf0f5ec41, 0x0d55, 0x4732, { 0xaf, 0x02, 0x44, 0x0a, 0x44, 0xa3, 0xcf, 0x0f, } } /*f0f5ec41-0d55-4732-af02-440a44a3cf0f*/, "Windows Server 2012 Standard",            EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN2012 },
	/* 015 */ { { 0x95fd1c83, 0x7df5, 0x494a, { 0xbe, 0x8b, 0x13, 0x00, 0xe1, 0xc9, 0xd1, 0xcd, } } /*95fd1c83-7df5-494a-be8b-1300e1c9d1cd*/, "Windows Server 2012 MultiPoint Premium",  EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN2012 },
	/* 016 */ { { 0x7d5486c7, 0xe120, 0x4771, { 0xb7, 0xf1, 0x7b, 0x56, 0xc6, 0xd3, 0x17, 0x0c, } } /*7d5486c7-e120-4771-b7f1-7b56c6d3170c*/, "Windows Server 2012 MultiPoint Standard", EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN2012 },
	/* 017 */ { { 0x00091344, 0x1ea4, 0x4f37, { 0xb7, 0x89, 0x01, 0x75, 0x0b, 0xa6, 0x98, 0x8c, } } /*00091344-1ea4-4f37-b789-01750ba6988c*/, "Windows Server 2012 R2 Datacenter",       EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN2012R2 },
	/* 018 */ { { 0xb3ca044e, 0xa358, 0x4d68, { 0x98, 0x83, 0xaa, 0xa2, 0x94, 0x1a, 0xca, 0x99, } } /*b3ca044e-a358-4d68-9883-aaa2941aca99*/, "Windows Server 2012 R2 Standard",         EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN2012R2 },
	/* 019 */ { { 0xb743a2be, 0x68d4, 0x4dd3, { 0xaf, 0x32, 0x92, 0x42, 0x5b, 0x7b, 0xb6, 0x23, } } /*b743a2be-68d4-4dd3-af32-92425b7bb623*/, "Windows Server 2012 R2 Cloud Storage",    EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN2012R2 },
	/* 020 */ { { 0x21db6ba4, 0x9a7b, 0x4a14, { 0x9e, 0x29, 0x64, 0xa6, 0x0c, 0x59, 0x30, 0x1d, } } /*21db6ba4-9a7b-4a14-9e29-64a60c59301d*/, "Windows Server 2012 R2 Essentials",       EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN2012R2 },
	/* 021 */ { { 0x2B9C337F, 0x7A1D, 0x4271, { 0x90, 0xA3, 0xC6, 0x85, 0x5A, 0x2B, 0x8A, 0x1C, } } /*2B9C337F-7A1D-4271-90A3-C6855A2B8A1C*/, "Windows 8.x Beta/Preview",                EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN8_BETA },
	/* 022 */ { { 0x631EAD72, 0xA8AB, 0x4DF8, { 0xBB, 0xDF, 0x37, 0x20, 0x29, 0x98, 0x9B, 0xDD, } } /*631EAD72-A8AB-4DF8-BBDF-372029989BDD*/, "Windows 8.x Beta/Preview ARM",            EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN8_BETA },
	/* 023 */ { { 0x81671aaf, 0x79d1, 0x4eb1, { 0xb0, 0x04, 0x8c, 0xbb, 0xe1, 0x73, 0xaf, 0xea, } } /*81671aaf-79d1-4eb1-b004-8cbbe173afea*/, "Windows 8.1 Enterprise",                  EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN81_VL },
	/* 024 */ { { 0x113e705c, 0xfa49, 0x48a4, { 0xbe, 0xea, 0x7d, 0xd8, 0x79, 0xb4, 0x6b, 0x14, } } /*113e705c-fa49-48a4-beea-7dd879b46b14*/, "Windows 8.1 Enterprise N",                EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN81_VL },
	/* 025 */ { { 0x096ce63d, 0x4fac, 0x48a9, { 0x82, 0xa9, 0x61, 0xae, 0x9e, 0x80, 0x0e, 0x5f, } } /*096ce63d-4fac-48a9-82a9-61ae9e800e5f*/, "Windows 8.1 Professional WMC",            EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN81_RETAIL },
	/* 026 */ { { 0xc06b6981, 0xd7fd, 0x4a35, { 0xb7, 0xb4, 0x05, 0x47, 0x42, 0xb7, 0xaf, 0x67, } } /*c06b6981-d7fd-4a35-b7b4-054742b7af67*/, "Windows 8.1 Professional",                EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN81_VL },
	/* 027 */ { { 0x7476d79f, 0x8e48, 0x49b4, { 0xab, 0x63, 0x4d, 0x0b, 0x81, 0x3a, 0x16, 0xe4, } } /*7476d79f-8e48-49b4-ab63-4d0b813a16e4*/, "Windows 8.1 Professional N",              EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN81_VL },
	/* 028 */ { { 0xfe1c3238, 0x432a, 0x43a1, { 0x8e, 0x25, 0x97, 0xe7, 0xd1, 0xef, 0x10, 0xf3, } } /*fe1c3238-432a-43a1-8e25-97e7d1ef10f3*/, "Windows 8.1 Core",                        EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN81_RETAIL },
	/* 029 */ { { 0x78558a64, 0xdc19, 0x43fe, { 0xa0, 0xd0, 0x80, 0x75, 0xb2, 0xa3, 0x70, 0xa3, } } /*78558a64-dc19-43fe-a0d0-8075b2a370a3*/, "Windows 8.1 Core N",                      EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN81_RETAIL },
	/* 030 */ { { 0xffee456a, 0xcd87, 0x4390, { 0x8e, 0x07, 0x16, 0x14, 0x6c, 0x67, 0x2f, 0xd0, } } /*ffee456a-cd87-4390-8e07-16146c672fd0*/, "Windows 8.1 Core ARM",                    EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN81_RETAIL },
	/* 031 */ { { 0xc72c6a1d, 0xf252, 0x4e7e, { 0xbd, 0xd1, 0x3f, 0xca, 0x34, 0x2a, 0xcb, 0x35, } } /*c72c6a1d-f252-4e7e-bdd1-3fca342acb35*/, "Windows 8.1 Core Single Language",        EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN81_RETAIL },
	/* 032 */ { { 0xdb78b74f, 0xef1c, 0x4892, { 0xab, 0xfe, 0x1e, 0x66, 0xb8, 0x23, 0x1d, 0xf6, } } /*db78b74f-ef1c-4892-abfe-1e66b8231df6*/, "Windows 8.1 Core Country Specific",       EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN81_RETAIL },
	/* 033 */ { { 0xa00018a3, 0xf20f, 0x4632, { 0xbf, 0x7c, 0x8d, 0xaa, 0x53, 0x51, 0xc9, 0x14, } } /*a00018a3-f20f-4632-bf7c-8daa5351c914*/, "Windows 8 Professional WMC",              EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN8_RETAIL },
	/* 034 */ { { 0xa98bcd6d, 0x5343, 0x4603, { 0x8a, 0xfe, 0x59, 0x08, 0xe4, 0x61, 0x11, 0x12, } } /*a98bcd6d-5343-4603-8afe-5908e4611112*/, "Windows 8 Professional",                  EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN8_VL },
	/* 035 */ { { 0xebf245c1, 0x29a8, 0x4daf, { 0x9c, 0xb1, 0x38, 0xdf, 0xc6, 0x08, 0xa8, 0xc8, } } /*ebf245c1-29a8-4daf-9cb1-38dfc608a8c8*/, "Windows 8 Professional N",                EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN8_VL },
	/* 036 */ { { 0x458e1bec, 0x837a, 0x45f6, { 0xb9, 0xd5, 0x92, 0x5e, 0xd5, 0xd2, 0x99, 0xde, } } /*458e1bec-837a-45f6-b9d5-925ed5d299de*/, "Windows 8 Enterprise",                    EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN8_VL },
	/* 037 */ { { 0xe14997e7, 0x800a, 0x4cf7, { 0xad, 0x10, 0xde, 0x4b, 0x45, 0xb5, 0x78, 0xdb, } } /*e14997e7-800a-4cf7-ad10-de4b45b578db*/, "Windows 8 Enterprise N",                  EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN8_VL },
	/* 038 */ { { 0xc04ed6bf, 0x55c8, 0x4b47, { 0x9f, 0x8e, 0x5a, 0x1f, 0x31, 0xce, 0xee, 0x60, } } /*c04ed6bf-55c8-4b47-9f8e-5a1f31ceee60*/, "Windows 8 Core",                          EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN8_RETAIL },
	/* 039 */ { { 0x197390a0, 0x65f6, 0x4a95, { 0xbd, 0xc4, 0x55, 0xd5, 0x8a, 0x3b, 0x02, 0x53, } } /*197390a0-65f6-4a95-bdc4-55d58a3b0253*/, "Windows 8 Core N",                        EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN8_RETAIL },
	/* 040 */ { { 0x9d5584a2, 0x2d85, 0x419a, { 0x98, 0x2c, 0xa0, 0x08, 0x88, 0xbb, 0x9d, 0xdf, } } /*9d5584a2-2d85-419a-982c-a00888bb9ddf*/, "Windows 8 Core Country Specific",         EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN8_RETAIL },
	/* 041 */ { { 0x8860fcd4, 0xa77b, 0x4a20, { 0x90, 0x45, 0xa1, 0x50, 0xff, 0x11, 0xd6, 0x09, } } /*8860fcd4-a77b-4a20-9045-a150ff11d609*/, "Windows 8 Core Single Language",          EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN8_RETAIL },
	/* 042 */ { { 0xae2ee509, 0x1b34, 0x41c0, { 0xac, 0xb7, 0x6d, 0x46, 0x50, 0x16, 0x89, 0x15, } } /*ae2ee509-1b34-41c0-acb7-6d4650168915*/, "Windows 7 Enterprise",                    EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN7 },
	/* 043 */ { { 0x1cb6d605, 0x11b3, 0x4e14, { 0xbb, 0x30, 0xda, 0x91, 0xc8, 0xe3, 0x98, 0x3a, } } /*1cb6d605-11b3-4e14-bb30-da91c8e3983a*/, "Windows 7 Enterprise N",                  EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN7 },
	/* 044 */ { { 0xb92e9980, 0xb9d5, 0x4821, { 0x9c, 0x94, 0x14, 0x0f, 0x63, 0x2f, 0x63, 0x12, } } /*b92e9980-b9d5-4821-9c94-140f632f6312*/, "Windows 7 Professional",                  EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN7 },
	/* 045 */ { { 0x54a09a0d, 0xd57b, 0x4c10, { 0x8b, 0x69, 0xa8, 0x42, 0xd6, 0x59, 0x0a, 0xd5, } } /*54a09a0d-d57b-4c10-8b69-a842d6590ad5*/, "Windows 7 Professional N",                EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN7 },
	/* 046 */ { { 0xcfd8ff08, 0xc0d7, 0x452b, { 0x9f, 0x60, 0xef, 0x5c, 0x70, 0xc3, 0x20, 0x94, } } /*cfd8ff08-c0d7-452b-9f60-ef5c70c32094*/, "Windows Vista Enterprise",                EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_VISTA },
	/* 047 */ { { 0xd4f54950, 0x26f2, 0x4fb4, { 0xba, 0x21, 0xff, 0xab, 0x16, 0xaf, 0xca, 0xde, } } /*d4f54950-26f2-4fb4-ba21-ffab16afcade*/, "Windows Vista Enterprise N",              EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_VISTA },
	/* 048 */ { { 0x4f3d1606, 0x3fea, 0x4c01, { 0xbe, 0x3c, 0x8d, 0x67, 0x1c, 0x40, 0x1e, 0x3b, } } /*4f3d1606-3fea-4c01-be3c-8d671c401e3b*/, "Windows Vista Business",                  EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_VISTA },
	/* 049 */ { { 0x2c682dc2, 0x8b68, 0x4f63, { 0xa1, 0x65, 0xae, 0x29, 0x1d, 0x4c, 0xf1, 0x38, } } /*2c682dc2-8b68-4f63-a165-ae291d4cf138*/, "Windows Vista Business N",                EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_VISTA },
	/* 050 */ { { 0xaa6dd3aa, 0xc2b4, 0x40e2, { 0xa5, 0x44, 0xa6, 0xbb, 0xb3, 0xf5, 0xc3, 0x95, } } /*aa6dd3aa-c2b4-40e2-a544-a6bbb3f5c395*/, "Windows ThinPC",                          EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN7 },
	/* 051 */ { { 0xdb537896, 0x376f, 0x48ae, { 0xa4, 0x92, 0x53, 0xd0, 0x54, 0x77, 0x73, 0xd0, } } /*db537896-376f-48ae-a492-53d0547773d0*/, "Windows Embedded POSReady 7",             EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN7 },
	/* 052 */ { { 0x0ab82d54, 0x47f4, 0x4acb, { 0x81, 0x8c, 0xcc, 0x5b, 0xf0, 0xec, 0xb6, 0x49, } } /*0ab82d54-47f4-4acb-818c-cc5bf0ecb649*/, "Windows Embedded Industry 8.1",           EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN81_VL },
	/* 053 */ { { 0xcd4e2d9f, 0x5059, 0x4a50, { 0xa9, 0x2d, 0x05, 0xd5, 0xbb, 0x12, 0x67, 0xc7, } } /*cd4e2d9f-5059-4a50-a92d-05d5bb1267c7*/, "Windows Embedded Industry E 8.1",         EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN81_VL },
	/* 054 */ { { 0xf7e88590, 0xdfc7, 0x4c78, { 0xbc, 0xcb, 0x6f, 0x38, 0x65, 0xb9, 0x9d, 0x1a, } } /*f7e88590-dfc7-4c78-bccb-6f3865b99d1a*/, "Windows Embedded Industry A 8.1",         EPID_WINDOWS,    APP_ID_WINDOWS,    KMS_ID_WIN81_VL },
	/* 055 */ { { 0x8ce7e872, 0x188c, 0x4b98, { 0x9d, 0x90, 0xf8, 0xf9, 0x0b, 0x7a, 0xad, 0x02, } } /*8ce7e872-188c-4b98-9d90-f8f90b7aad02*/, "Office Access 2010",                      EPID_OFFICE2010, APP_ID_OFFICE2010, KMS_ID_OFFICE2010 },
	/* 056 */ { { 0xcee5d470, 0x6e3b, 0x4fcc, { 0x8c, 0x2b, 0xd1, 0x74, 0x28, 0x56, 0x8a, 0x9f, } } /*cee5d470-6e3b-4fcc-8c2b-d17428568a9f*/, "Office Excel 2010",                       EPID_OFFICE2010, APP_ID_OFFICE2010, KMS_ID_OFFICE2010 },
	/* 057 */ { { 0x8947d0b8, 0xc33b, 0x43e1, { 0x8c, 0x56, 0x9b, 0x67, 0x4c, 0x05, 0x28, 0x32, } } /*8947d0b8-c33b-43e1-8c56-9b674c052832*/, "Office Groove 2010",                      EPID_OFFICE2010, APP_ID_OFFICE2010, KMS_ID_OFFICE2010 },
	/* 058 */ { { 0xca6b6639, 0x4ad6, 0x40ae, { 0xa5, 0x75, 0x14, 0xde, 0xe0, 0x7f, 0x64, 0x30, } } /*ca6b6639-4ad6-40ae-a575-14dee07f6430*/, "Office InfoPath 2010",                    EPID_OFFICE2010, APP_ID_OFFICE2010, KMS_ID_OFFICE2010 },
	/* 059 */ { { 0x09ed9640, 0xf020, 0x400a, { 0xac, 0xd8, 0xd7, 0xd8, 0x67, 0xdf, 0xd9, 0xc2, } } /*09ed9640-f020-400a-acd8-d7d867dfd9c2*/, "Office Mondo 2010",                       EPID_OFFICE2010, APP_ID_OFFICE2010, KMS_ID_OFFICE2010 },
	/* 060 */ { { 0xef3d4e49, 0xa53d, 0x4d81, { 0xa2, 0xb1, 0x2c, 0xa6, 0xc2, 0x55, 0x6b, 0x2c, } } /*ef3d4e49-a53d-4d81-a2b1-2ca6c2556b2c*/, "Office Mondo 2010",                       EPID_OFFICE2010, APP_ID_OFFICE2010, KMS_ID_OFFICE2010 },
	/* 061 */ { { 0xab586f5c, 0x5256, 0x4632, { 0x96, 0x2f, 0xfe, 0xfd, 0x8b, 0x49, 0xe6, 0xf4, } } /*ab586f5c-5256-4632-962f-fefd8b49e6f4*/, "Office OneNote 2010",                     EPID_OFFICE2010, APP_ID_OFFICE2010, KMS_ID_OFFICE2010 },
	/* 062 */ { { 0xecb7c192, 0x73ab, 0x4ded, { 0xac, 0xf4, 0x23, 0x99, 0xb0, 0x95, 0xd0, 0xcc, } } /*ecb7c192-73ab-4ded-acf4-2399b095d0cc*/, "Office OutLook 2010",                     EPID_OFFICE2010, APP_ID_OFFICE2010, KMS_ID_OFFICE2010 },
	/* 063 */ { { 0x45593b1d, 0xdfb1, 0x4e91, { 0xbb, 0xfb, 0x2d, 0x5d, 0x0c, 0xe2, 0x22, 0x7a, } } /*45593b1d-dfb1-4e91-bbfb-2d5d0ce2227a*/, "Office PowerPoint 2010",                  EPID_OFFICE2010, APP_ID_OFFICE2010, KMS_ID_OFFICE2010 },
	/* 064 */ { { 0xdf133ff7, 0xbf14, 0x4f95, { 0xaf, 0xe3, 0x7b, 0x48, 0xe7, 0xe3, 0x31, 0xef, } } /*df133ff7-bf14-4f95-afe3-7b48e7e331ef*/, "Office Project Pro 2010",                 EPID_OFFICE2010, APP_ID_OFFICE2010, KMS_ID_OFFICE2010 },
	/* 065 */ { { 0x5dc7bf61, 0x5ec9, 0x4996, { 0x9c, 0xcb, 0xdf, 0x80, 0x6a, 0x2d, 0x0e, 0xfe, } } /*5dc7bf61-5ec9-4996-9ccb-df806a2d0efe*/, "Office Project Standard 2010",            EPID_OFFICE2010, APP_ID_OFFICE2010, KMS_ID_OFFICE2010 },
	/* 066 */ { { 0xb50c4f75, 0x599b, 0x43e8, { 0x8d, 0xcd, 0x10, 0x81, 0xa7, 0x96, 0x72, 0x41, } } /*b50c4f75-599b-43e8-8dcd-1081a7967241*/, "Office Publisher 2010",                   EPID_OFFICE2010, APP_ID_OFFICE2010, KMS_ID_OFFICE2010 },
	/* 067 */ { { 0x92236105, 0xbb67, 0x494f, { 0x94, 0xc7, 0x7f, 0x7a, 0x60, 0x79, 0x29, 0xbd, } } /*92236105-bb67-494f-94c7-7f7a607929bd*/, "Office Visio Premium 2010",               EPID_OFFICE2010, APP_ID_OFFICE2010, KMS_ID_OFFICE2010 },
	/* 068 */ { { 0xe558389c, 0x83c3, 0x4b29, { 0xad, 0xfe, 0x5e, 0x4d, 0x7f, 0x46, 0xc3, 0x58, } } /*e558389c-83c3-4b29-adfe-5e4d7f46c358*/, "Office Visio Pro 2010",                   EPID_OFFICE2010, APP_ID_OFFICE2010, KMS_ID_OFFICE2010 },
	/* 069 */ { { 0x9ed833ff, 0x4f92, 0x4f36, { 0xb3, 0x70, 0x86, 0x83, 0xa4, 0xf1, 0x32, 0x75, } } /*9ed833ff-4f92-4f36-b370-8683a4f13275*/, "Office Visio Standard 2010",              EPID_OFFICE2010, APP_ID_OFFICE2010, KMS_ID_OFFICE2010 },
	/* 070 */ { { 0x2d0882e7, 0xa4e7, 0x423b, { 0x8c, 0xcc, 0x70, 0xd9, 0x1e, 0x01, 0x58, 0xb1, } } /*2d0882e7-a4e7-423b-8ccc-70d91e0158b1*/, "Office Word 2010",                        EPID_OFFICE2010, APP_ID_OFFICE2010, KMS_ID_OFFICE2010 },
	/* 071 */ { { 0x6f327760, 0x8c5c, 0x417c, { 0x9b, 0x61, 0x83, 0x6a, 0x98, 0x28, 0x7e, 0x0c, } } /*6f327760-8c5c-417c-9b61-836a98287e0c*/, "Office Professional Plus 2010",           EPID_OFFICE2010, APP_ID_OFFICE2010, KMS_ID_OFFICE2010 },
	/* 072 */ { { 0x9da2a678, 0xfb6b, 0x4e67, { 0xab, 0x84, 0x60, 0xdd, 0x6a, 0x9c, 0x81, 0x9a, } } /*9da2a678-fb6b-4e67-ab84-60dd6a9c819a*/, "Office Standard 2010",                    EPID_OFFICE2010, APP_ID_OFFICE2010, KMS_ID_OFFICE2010 },
	/* 073 */ { { 0xea509e87, 0x07a1, 0x4a45, { 0x9e, 0xdc, 0xeb, 0xa5, 0xa3, 0x9f, 0x36, 0xaf, } } /*ea509e87-07a1-4a45-9edc-eba5a39f36af*/, "Office Small Business Basics 2010",       EPID_OFFICE2010, APP_ID_OFFICE2010, KMS_ID_OFFICE2010 },
	/* 074 */ { { 0x6ee7622c, 0x18d8, 0x4005, { 0x9f, 0xb7, 0x92, 0xdb, 0x64, 0x4a, 0x27, 0x9b, } } /*6ee7622c-18d8-4005-9fb7-92db644a279b*/, "Office Access 2013",                      EPID_OFFICE2013, APP_ID_OFFICE2013, KMS_ID_OFFICE2013 },
	/* 075 */ { { 0xf7461d52, 0x7c2b, 0x43b2, { 0x87, 0x44, 0xea, 0x95, 0x8e, 0x0b, 0xd0, 0x9a, } } /*f7461d52-7c2b-43b2-8744-ea958e0bd09a*/, "Office Excel 2013",                       EPID_OFFICE2013, APP_ID_OFFICE2013, KMS_ID_OFFICE2013 },
	/* 076 */ { { 0xa30b8040, 0xd68a, 0x423f, { 0xb0, 0xb5, 0x9c, 0xe2, 0x92, 0xea, 0x5a, 0x8f, } } /*a30b8040-d68a-423f-b0b5-9ce292ea5a8f*/, "Office InfoPath 2013",                    EPID_OFFICE2013, APP_ID_OFFICE2013, KMS_ID_OFFICE2013 },
	/* 077 */ { { 0x1b9f11e3, 0xc85c, 0x4e1b, { 0xbb, 0x29, 0x87, 0x9a, 0xd2, 0xc9, 0x09, 0xe3, } } /*1b9f11e3-c85c-4e1b-bb29-879ad2c909e3*/, "Office Lync 2013",                        EPID_OFFICE2013, APP_ID_OFFICE2013, KMS_ID_OFFICE2013 },
	/* 078 */ { { 0xdc981c6b, 0xfc8e, 0x420f, { 0xaa, 0x43, 0xf8, 0xf3, 0x3e, 0x5c, 0x09, 0x23, } } /*dc981c6b-fc8e-420f-aa43-f8f33e5c0923*/, "Office Mondo 2013",                       EPID_OFFICE2013, APP_ID_OFFICE2013, KMS_ID_OFFICE2013 },
	/* 079 */ { { 0xefe1f3e6, 0xaea2, 0x4144, { 0xa2, 0x08, 0x32, 0xaa, 0x87, 0x2b, 0x65, 0x45, } } /*efe1f3e6-aea2-4144-a208-32aa872b6545*/, "Office OneNote 2013",                     EPID_OFFICE2013, APP_ID_OFFICE2013, KMS_ID_OFFICE2013 },
	/* 080 */ { { 0x771c3afa, 0x50c5, 0x443f, { 0xb1, 0x51, 0xff, 0x25, 0x46, 0xd8, 0x63, 0xa0, } } /*771c3afa-50c5-443f-b151-ff2546d863a0*/, "Office OutLook 2013",                     EPID_OFFICE2013, APP_ID_OFFICE2013, KMS_ID_OFFICE2013 },
	/* 081 */ { { 0x8c762649, 0x97d1, 0x4953, { 0xad, 0x27, 0xb7, 0xe2, 0xc2, 0x5b, 0x97, 0x2e, } } /*8c762649-97d1-4953-ad27-b7e2c25b972e*/, "Office PowerPoint 2013",                  EPID_OFFICE2013, APP_ID_OFFICE2013, KMS_ID_OFFICE2013 },
	/* 082 */ { { 0x4a5d124a, 0xe620, 0x44ba, { 0xb6, 0xff, 0x65, 0x89, 0x61, 0xb3, 0x3b, 0x9a, } } /*4a5d124a-e620-44ba-b6ff-658961b33b9a*/, "Office Project Pro 2013",                 EPID_OFFICE2013, APP_ID_OFFICE2013, KMS_ID_OFFICE2013 },
	/* 083 */ { { 0x427a28d1, 0xd17c, 0x4abf, { 0xb7, 0x17, 0x32, 0xc7, 0x80, 0xba, 0x6f, 0x07, } } /*427a28d1-d17c-4abf-b717-32c780ba6f07*/, "Office Project Standard 2013",            EPID_OFFICE2013, APP_ID_OFFICE2013, KMS_ID_OFFICE2013 },
	/* 084 */ { { 0x00c79ff1, 0x6850, 0x443d, { 0xbf, 0x61, 0x71, 0xcd, 0xe0, 0xde, 0x30, 0x5f, } } /*00c79ff1-6850-443d-bf61-71cde0de305f*/, "Office Publisher 2013",                   EPID_OFFICE2013, APP_ID_OFFICE2013, KMS_ID_OFFICE2013 },
	/* 085 */ { { 0xac4efaf0, 0xf81f, 0x4f61, { 0xbd, 0xf7, 0xea, 0x32, 0xb0, 0x2a, 0xb1, 0x17, } } /*ac4efaf0-f81f-4f61-bdf7-ea32b02ab117*/, "Office Visio Standard 2013",              EPID_OFFICE2013, APP_ID_OFFICE2013, KMS_ID_OFFICE2013 },
	/* 086 */ { { 0xe13ac10e, 0x75d0, 0x4aff, { 0xa0, 0xcd, 0x76, 0x49, 0x82, 0xcf, 0x54, 0x1c, } } /*e13ac10e-75d0-4aff-a0cd-764982cf541c*/, "Office Visio Pro 2013",                   EPID_OFFICE2013, APP_ID_OFFICE2013, KMS_ID_OFFICE2013 },
	/* 087 */ { { 0xd9f5b1c6, 0x5386, 0x495a, { 0x88, 0xf9, 0x9a, 0xd6, 0xb4, 0x1a, 0xc9, 0xb3, } } /*d9f5b1c6-5386-495a-88f9-9ad6b41ac9b3*/, "Office Word 2013",                        EPID_OFFICE2013, APP_ID_OFFICE2013, KMS_ID_OFFICE2013 },
	/* 088 */ { { 0xb322da9c, 0xa2e2, 0x4058, { 0x9e, 0x4e, 0xf5, 0x9a, 0x69, 0x70, 0xbd, 0x69, } } /*b322da9c-a2e2-4058-9e4e-f59a6970bd69*/, "Office Professional Plus 2013",           EPID_OFFICE2013, APP_ID_OFFICE2013, KMS_ID_OFFICE2013 },
	/* 089 */ { { 0xb13afb38, 0xcd79, 0x4ae5, { 0x9f, 0x7f, 0xee, 0xd0, 0x58, 0xd7, 0x50, 0xca, } } /*b13afb38-cd79-4ae5-9f7f-eed058d750ca*/, "Office Standard 2013",                    EPID_OFFICE2013, APP_ID_OFFICE2013, KMS_ID_OFFICE2013 },
	/* 090 */ { { 0x00000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }, NULL, NULL, 0, 0 }
};


// necessary because other .c files cannot access _countof()
__pure uint8_t GetExtendedProductListSize()
{
	return _countof(ExtendedProductList) - 1;
}

#endif

#ifndef NO_RANDOM_EPID
// HostType and OSBuild
static const struct KMSHostOS { uint16_t Type; uint16_t Build; } HostOS[] =
{
	{ 55041, 6002 }, // Windows Server 2008 SP2
    { 55041, 7601 }, // Windows Server 2008 R2 SP1
    {  5426, 9200 }, // Windows Server 2012
    {  6401, 9600 }, // Windows Server 2012 R2
};

// GroupID and PIDRange
static const struct PKEYCONFIG { uint16_t GroupID; uint32_t RangeMin; uint32_t RangeMax; } pkeyconfig[] = {
    { 206, 152000000, 191999999 }, // Windows Server 2012 KMS Host pkeyconfig
    { 206, 271000000, 310999999 }, // Windows Server 2012 R2 KMS Host pkeyconfig
    {  96, 199000000, 217999999 }, // Office2010 KMS Host pkeyconfig
    { 206, 234000000, 255999999 }, // Office2013 KMS Host pkeyconfig
};

// Valid language identifiers to be used in the ePID
static const uint16_t LcidList[] = {
	1078, 1052, 1025, 2049, 3073, 4097, 5121, 6145, 7169, 8193, 9217, 10241, 11265, 12289, 13313, 14337, 15361, 16385,
	1067, 1068, 2092, 1069, 1059, 1093, 5146, 1026, 1027, 1028, 2052, 3076, 4100, 5124, 1050, 4122, 1029, 1030, 1125, 1043, 2067,
	1033, 2057, 3081, 4105, 5129, 6153, 7177, 8201, 9225, 10249, 11273, 12297, 13321, 1061, 1080, 1065, 1035, 1036, 2060,
	3084, 4108, 5132, 6156, 1079, 1110, 1031, 2055, 3079, 4103, 5127, 1032, 1095, 1037, 1081, 1038, 1039, 1057, 1040, 2064, 1041, 1099,
	1087, 1111, 1042, 1088, 1062, 1063, 1071, 1086, 2110, 1100, 1082, 1153, 1102, 1104, 1044, 2068, 1045, 1046, 2070,
	1094, 1131, 2155, 3179, 1048, 1049, 9275, 4155, 5179, 3131, 1083, 2107, 8251, 6203, 7227, 1103, 2074, 6170, 3098,
	7194, 1051, 1060, 1034, 2058, 3082, 4106, 5130, 6154, 7178, 8202, 9226, 10250, 11274, 12298, 13322, 14346, 15370, 16394,
	17418, 18442, 19466, 20490, 1089, 1053, 2077, 1114, 1097, 1092, 1098, 1054, 1074, 1058, 1056, 1091, 2115, 1066, 1106, 1076, 1077
};


#ifdef _PEDANTIC
uint16_t IsValidLcid(const uint16_t Lcid)
{
	uint16_t i;

	for (i = 0; i < _countof(LcidList); i++)
	{
		if (Lcid == LcidList[i]) return Lcid;
	}

	return 0;
}
#endif // _PEDANTIC
#endif // NO_RANDOM_EPID

const char* GetProductName(GUID guid, const KmsIdList* List, int *i)
{
	for (*i = 0; List[*i].name != NULL; (*i)++)
	{
		if (guid.Data1 == LE32(List[*i].guid.Data1) &&
			guid.Data2 == LE16(List[*i].guid.Data2) &&
			guid.Data3 == LE16(List[*i].guid.Data3) &&
			*((uint64_t*)&guid.Data4) == *((uint64_t*)&List[*i].guid.Data4))

				return List[*i].name;
	}

	return "Unknown";
}

#ifndef NO_RANDOM_EPID
static char RandomPid[][PID_BUFFER_SIZE] =
{
		"", "", ""
};

static char* itoc(char* c, int i, int digits)
{
	char formatString[8];
	if (digits < 0 || digits > 9) digits = 0;
	strcpy(formatString,"%");

	if (digits)
	{
		formatString[1] = '0';
		formatString[2] = digits | 0x30;
		formatString[3] = 0;
	}

	strcat(formatString, "u");
	sprintf(c, formatString, i);
	return c;
}

static void GenerateRandomPid(GUID guid, char* szPid, int serverType, int16_t lang)
{
	if (serverType < 0 || serverType >= (int)_countof(HostOS))
		serverType = rand() % (int)_countof(HostOS);

	int clientApp;
	char numberBuffer[12];

	strcpy(szPid, itoc(numberBuffer, HostOS[serverType].Type, 5));
	strcat(szPid, "-");

	int index;
	strcpy(numberBuffer, GetProductName(guid, AppList, &index));

	if (!strncmp(FRIENDLY_NAME_OFFICE2013, numberBuffer, sizeof(numberBuffer)))
	{
		clientApp = 3;
	}
	else if (!strncmp(FRIENDLY_NAME_OFFICE2010, numberBuffer, sizeof(numberBuffer)))
	{
		clientApp = 2;
	}
	else
	{
		clientApp = serverType == 3 /*change if HostOS changes*/ ? 1 : 0;
	}

	strcat(szPid, itoc(numberBuffer, pkeyconfig[clientApp].GroupID, 5));
	strcat(szPid, "-");

	int keyId = (rand32() % (pkeyconfig[clientApp].RangeMax - pkeyconfig[clientApp].RangeMin)) + pkeyconfig[clientApp].RangeMin;
	strcat(szPid, itoc(numberBuffer, keyId / 1000000, 3));
	strcat(szPid, "-");
	strcat(szPid, itoc(numberBuffer, keyId % 1000000, 6));
	strcat(szPid, "-03-");

	if (lang < 0) lang = LcidList[rand() % _countof(LcidList)];
	strcat(szPid, itoc(numberBuffer, lang, 0));
	strcat(szPid, "-");

	strcat(szPid, itoc(numberBuffer, HostOS[serverType].Build, 0));
	strcat(szPid, ".0000-");

	time_t minTime = (time_t)1382029200LL; // RelaseDate Win2012R2
	time_t maxTime, kmsTime;
	time(&maxTime);
	kmsTime = (rand32() % (maxTime - minTime)) + minTime;

	struct tm *pidTime;
	pidTime = gmtime(&kmsTime);

	strcat(szPid, itoc(numberBuffer, pidTime->tm_yday, 3));
	strcat(szPid, itoc(numberBuffer, pidTime->tm_year + 1900, 4));
}

void RandomPidInit()
{
	unsigned int i;
	srand((unsigned int)time(NULL));

	int serverType = rand() % _countof(HostOS);
	int16_t lang = Lcid ? Lcid : LcidList[rand() % _countof(LcidList)];

	for (i = 0; i < _countof(RandomPid); i++)
	{
		GenerateRandomPid(AppList[i].guid, RandomPid[i], serverType, lang);
	}
}

#endif // NO_RANDOM_EPID

#ifndef NO_INI_FILE
static __pure int isControlCharOrSlash(const char c)
{
	if ((unsigned char)c < '!') return !0;
	if (c == '/') return !0;
	return 0;
}

#endif

void Hex2bin(BYTE *const bin, const char *hex, const size_t maxbin)
{
	static const char *const hexdigits = "0123456789ABCDEF";
	char* nextchar;
	size_t i;

	for (i = 0; (i < 16) && utf8_to_ucs2_char((const unsigned char*)hex, (const unsigned char**)&nextchar) != (WCHAR)-1; hex = nextchar)
	{
		const char* pos = strchr(hexdigits, toupper((int)*hex));
		if (!pos) continue;

		if (!(i & 1)) bin[i >> 1] = 0;
		bin[i >> 1] |= (char)(pos - hexdigits);
		if (!(i & 1)) bin[i >> 1] <<= 4;
		i++;
		if (i >> 1 > maxbin) break;
	}
}


#ifndef NO_LOG
static void LogRequest(const REQUEST *const Request)
{
	const char *productName;
	char clientname[64];
	int index;

	#ifndef NO_EXTENDED_PRODUCT_LIST
	productName = GetProductName(Request->SkuId, ExtendedProductList, &index);
	if (++index >= (int)_countof(ExtendedProductList))
	#endif // NO_EXTENDED_PRODUCT_LIST
	{
		#ifndef NO_BASIC_PRODUCT_LIST
		productName = GetProductName(Request->KmsId, ProductList, &index);
		if (++index >= (int)_countof(ProductList))
		#endif // NO_BASIC_PRODUCT_LIST
		{
			productName = GetProductName(Request->AppId, AppList, &index);
		}
	}

	#ifndef NO_VERBOSE_LOG
	if (logverbose)
	{
		logger("<<< Incoming KMS request\n");
		LogRequestVerbose(Request, &logger);
	}
	else
	{
	#endif // NO_VERBOSE_LOG
		ucs2_to_utf8(Request->WorkstationName, clientname, 64, 64);
		logger("KMS v%i.%i request from %s for %s\n", LE16(Request->MajorVer), LE16(Request->MinorVer), clientname, productName);
	#ifndef NO_VERBOSE_LOG
	}
	#endif // NO_VERBOSE_LOG
}
#endif // NO_LOG

static void GetEpidFromString(RESPONSE *const Response, const char *const pid)
{
	size_t length = utf8_to_ucs2(Response->KmsPID, pid, PID_BUFFER_SIZE, PID_BUFFER_SIZE * 3);
	Response->KmsPIDLen = LE32(((unsigned int )length + 1) << 1);
}

#ifndef NO_INI_FILE

static BOOL ReadUtf8Epid(RESPONSE *const Response, const char **s)
{
	unsigned int i;
	while ( **s && isspace((int)**s) ) (*s)++;

	for (i = 0; !isControlCharOrSlash(**s) && i < PID_BUFFER_SIZE - 1; i++)
	{
		Response->KmsPID[i] = utf8_to_ucs2_char((const unsigned char*)*s, (const unsigned char**)s);
		if (Response->KmsPID[i] == (WCHAR)-1 || i > _countof(Response->KmsPID) - 2)	return 0;
	}

	Response->KmsPID[i] = 0;
	Response->KmsPIDLen = LE32(((unsigned int)++i) << 1);

	return !0;
}

static void ReadHwId(BYTE *const HwId, const char **s)
{
	while ( **s && isspace((int)**s) ) (*s)++;

	if (HwId && **s == '/')
	{
		Hex2bin(HwId, *s+1, 8);
	}
}

static BOOL CheckGuid(const char *const str_guid, const size_t size, const char **s)
{
	unsigned int  i;

	while ( **s && isspace((int)**s) ) (*s)++;

	for (i = 0; i < size && ('-' == **s || str_guid[ i++ ] == tolower((int)**s)); (*s)++);
	if ( size != i ) return 0;

	while ( **s && isspace((int)**s) ) (*s)++;
	if ( *(*s)++ != '=' ) return 0;

	return !0;
}


////TODO: Read ini file at startup and add a SIGHUP handler to re-read it
static BOOL GetEpidFromIniFile(const REQUEST *const Request, RESPONSE *const Response, BYTE *const HwId, char const** EpidSource)
{
	char  str_guid[33], str_file[256];
	const char *s;

	FILE *restrict _f;
	BOOL result = FALSE;

	if ( !fn_ini || !(_f = fopen(fn_ini, "r") )) return FALSE;

	*EpidSource = fn_ini;

	if ( sizeof(str_guid)-1 != snprintf(str_guid, sizeof(str_guid),
			"%08x%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x",
			(uint32_t)LE32( Request->AppId.Data1 ),
			(uint32_t)LE16( Request->AppId.Data2 ),
			(uint32_t)LE16( Request->AppId.Data3 ),
			Request->AppId.Data4[0], Request->AppId.Data4[1],
			Request->AppId.Data4[2], Request->AppId.Data4[3],
			Request->AppId.Data4[4], Request->AppId.Data4[5],
			Request->AppId.Data4[6], Request->AppId.Data4[7] ))
		return FALSE;

	while ( (s = fgets(str_file, sizeof(str_file), _f)) )
	{
		if (!CheckGuid(str_guid, sizeof(str_guid), &s)) continue;
		if (!ReadUtf8Epid(Response, &s)) break;

		result = TRUE;
		if (LE16(Request->MajorVer) > 5 && !CommandLineHwId) ReadHwId(HwId, &s);
		break;
	}

	fclose(_f);
	return result;
}
#endif // NO_INI_FILE


static void GetRandomOrStaticEpid(const REQUEST *const Request, RESPONSE *const Response, char const** EpidSource)
{
	const char *restrict pid;
	#ifndef NO_LOG
	#endif

	#ifndef NO_RANDOM_EPID

	if (RandomizationLevel == 2)
	{
		char szPid[PID_BUFFER_SIZE];
		GenerateRandomPid(Request->AppId, szPid, -1, Lcid ? Lcid : -1);
		pid = szPid;
	}
	else
	{
		int index;
		GetProductName(Request->AppId, AppList, &index);
		if ((unsigned int)index >= _countof(AppList) - 1) index = 0;
		pid = RandomizationLevel ? RandomPid[index] : AppList[index].pid;
	}

	#ifndef NO_LOG
	static const char* pidType[] = { "vlmcsd default", "randomized at program start", "randomized at every request" };
	*EpidSource = pidType[RandomizationLevel];
	#endif // NO_LOG

	#else // NO_RANDOM_PID
	int index;
	GetProductName(Request->AppId, AppList, &index);
	if (index >= _countof(AppList) - 1) index = 0;
	pid = AppList[index].pid;
	#ifndef NO_LOG
	*EpidSource = "vlmcsd default";
	#endif // NO_LOG
	#endif // NO_RANDOM_EPID

	GetEpidFromString(Response, pid);
}


#ifndef NO_CL_PIDS
static BOOL GetEpidFromCommandLine(const REQUEST *const Request, RESPONSE *const Response, BYTE *const HwId, char const** EpidSource)
{
	int index;

	if (LE16(Request->MajorVer) > 5 && CommandLineHwId) Hex2bin(HwId, CommandLineHwId, 8);

	GetProductName(Request->AppId, AppList, &index);

	if ((unsigned int)index >= _countof(AppList) - 1) return FALSE;
	const char *ePid = CommandLineEpid[index];
	if (ePid == NULL) return FALSE;

	GetEpidFromString(Response, ePid);

	*EpidSource = "command line";

	return TRUE;
}
#endif // NO_CL_PIDS


#if !defined(NO_LOG) && defined(_PEDANTIC)
static BOOL CheckVersion4Uuid(const GUID *const guid, const char *const szGuidName)
{
	if (LE16(guid->Data3) >> 12 != 4 || guid->Data4[0] >> 6 != 2)
	{
		logger("Warning: %s does not conform to version 4 UUID according to RFC 4122\n", szGuidName);
		return FALSE;
	}
	return TRUE;
}


static void CheckRequest(const REQUEST *const Request)
{
	CheckVersion4Uuid(&Request->ClientMachineId, "Client machine ID");
	CheckVersion4Uuid(&Request->AppId, "Application ID");
	CheckVersion4Uuid(&Request->KmsId, "Server SKU ID");
	CheckVersion4Uuid(&Request->SkuId, "Client SKU ID");

	if (LE32(Request->IsClientVM) > 1)
		logger("Warning: Virtual Machine field in request must be 0 or 1 but is %u\n", LE32(Request->IsClientVM));

	if (LE32(Request->LicenseStatus) > 6 )
		logger("Warning: License status must be between 0 and 6 but is %u\n", LE32(Request->LicenseStatus));
}
#endif // !defined(NO_LOG) && defined(_PEDANTIC)


static BOOL CreateResponseBase(const REQUEST *const Request, RESPONSE *const Response, BYTE *const HwId)
{
	const char *EpidSource;
	#ifndef NO_LOG
	LogRequest(Request);
	#ifdef _PEDANTIC
	CheckRequest(Request);
	#endif // _PEDANTIC
	#endif // NO_LOG

	#ifndef NO_CL_PIDS
	if (!GetEpidFromCommandLine(Request, Response, HwId, &EpidSource))
	#endif // NO_CL_PIDS

	#ifndef NO_INI_FILE
	if (!GetEpidFromIniFile(Request, Response, HwId, &EpidSource))
	#endif // NO_INI_FILE

	GetRandomOrStaticEpid(Request, Response, &EpidSource);

	Response->Version = Request->Version;

	memcpy(&Response->ClientMachineId, &Request->ClientMachineId, sizeof(GUID));
	memcpy(&Response->TimeStamp, &Request->TimeStamp, sizeof(FILETIME));

	Response->ActivatedMachines  = LE32(LE32(Request->MinimumClients) << 1);
	Response->ActivationInterval = LE32(ActivationInterval);
	Response->RenewalInterval    = LE32(RenewalInterval);

	#ifndef NO_LOG

	char utf8pid[PID_BUFFER_SIZE * 3];
	ucs2_to_utf8(Response->KmsPID, utf8pid, PID_BUFFER_SIZE, PID_BUFFER_SIZE * 3);

	#ifndef NO_VERBOSE_LOG
	if (!logverbose)
	{
	#endif // NO_VERBOSE_LOG
		logger("Sending ePID (%s): %s\n", EpidSource, utf8pid);
	#ifndef NO_VERBOSE_LOG
	}
	else
	{
		logger(">>> Sending response, ePID source = %s\n", EpidSource);
		LogResponseVerbose(utf8pid, HwId, Response, &logger);
	}
	#endif // NO_VERBOSE_LOG
	#endif // NO_LOG

	return !0;
}


void Get16RandomBytes(void* ptr)
{
	int i;
	for (i = 0; i < 4; i++)	((DWORD*)ptr)[i] = rand32();
}


#define V4_PRE_EPID_SIZE (sizeof(rb->Version) + sizeof(rb->KmsPIDLen))
#define V4_POST_EPID_SIZE (sizeof(rb->ClientMachineId) + sizeof(rb->TimeStamp) + sizeof(rb->ActivatedMachines) + sizeof(rb->ActivationInterval) + sizeof(rb->RenewalInterval))


size_t CreateResponseV4(REQUEST_V4 *const Request, BYTE *const response_data)
{
	RESPONSE_V4 Response;
	RESPONSE* rb = &Response.ResponseBase;

	if ( !CreateResponseBase(&Request->RequestBase, rb, NULL) ) return 0;

	unsigned char* current = response_data;
	int copySize = V4_PRE_EPID_SIZE + LE32(rb->KmsPIDLen);
	memcpy(current, rb, copySize);
	current += copySize;

	copySize = V4_POST_EPID_SIZE;
	memcpy(current, &rb->ClientMachineId, copySize);
	current += copySize;

	// Generate Hash Signature
	size_t size = current - response_data;

	AesCmacV4(response_data, size, current);

	return current + sizeof(Response.Hash) - response_data;
}

// Workaround for buggy GCC 4.2/4.3
#if defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ < 4)
__attribute__((noinline))
#endif
__pure static uint64_t TimestampInterval(void *ts)
{
	return LE64(( GET_UA64LE(ts) / TIME_C1 ) * TIME_C2 + TIME_C3 );
}


static int CreateV6Hmac(RESPONSE_V6 *const Response, const AesCtx *const aes, BYTE *const encrypt_start, const size_t encryptSize, BYTE *const current, int_fast8_t tolerance)
{
	BYTE temp[32];
	#define halfTempSize (sizeof(temp) / 2)
	Sha256HmacCtx hmac;
	uint64_t ts;

	ts = LE64(LE64(TimestampInterval(&Response->ResponseBase.TimeStamp)) + tolerance * TIME_C1);

	Sha256((BYTE*) &ts, sizeof(ts), temp);

	if (!Sha256HmacInit(&hmac, temp + halfTempSize, halfTempSize)) return FALSE;

	memcpy(temp, Response->Salt, halfTempSize);
	AesDecryptBlock(aes, temp);

	if (!Sha256HmacUpdate(&hmac, temp, halfTempSize) ||
			!Sha256HmacUpdate(&hmac, encrypt_start, encryptSize - sizeof(Response->Hmac)) ||
			!Sha256HmacFinish(&hmac, temp))
		return FALSE;

	memcpy(current - sizeof(Response->Hmac), temp + halfTempSize, halfTempSize);
	return TRUE;
}


#define V6_DECRYPT_SIZE (sizeof(Request->Salt) + sizeof(Request->RequestBase) + sizeof(Request->Pad))
#define V6_UNENCRYPTED_SIZE (sizeof(Response.Version) + sizeof(Response.Salt))
#define V6_PRE_EPID_SIZE (V6_UNENCRYPTED_SIZE + sizeof(rb->Version) + sizeof(rb->KmsPIDLen))
#define V5_POST_EPID_SIZE (sizeof(rb->ClientMachineId) + sizeof(rb->TimeStamp) + sizeof(rb->ActivatedMachines) + sizeof(rb->ActivationInterval) + sizeof(rb->RenewalInterval) + sizeof(Response.Rand) + sizeof(Response.Hash))
#define V6_POST_EPID_SIZE (V5_POST_EPID_SIZE + sizeof(Response.HwId) + sizeof(Response.XorSalts) + sizeof(Response.Hmac))


size_t CreateResponseV6(REQUEST_V6 *restrict Request, BYTE *const response_data)
{
	RESPONSE_V6 Response;
	RESPONSE* rb = &Response.ResponseBase;

	#ifdef _DEBUG
		RESPONSE_V6_DEBUG* xxx = (RESPONSE_V6_DEBUG*)response_data;
	#endif

	static const BYTE DefaultHwid[8] = { HWID };
	uint16_t MajorVer = LE16(Request->MajorVer);
	AesCtx  aes;

	if (MajorVer == 5)
	{
		AesInitKey(&aes, AesKeyV5, FALSE, AES_KEY_BYTES);
		memcpy(&Response, Request, sizeof(Request->Version) + sizeof(Response.Salt));
	}
	else
	{
		AesInitKey(&aes, AesKeyV6, TRUE, AES_KEY_BYTES);
		Response.Version = Request->Version;
		Get16RandomBytes(Response.Salt);
	}

	AesDecryptCbc(&aes, NULL, Request->Salt, V6_DECRYPT_SIZE);
	memcpy(Response.HwId, DefaultHwid, sizeof(Response.HwId));

	if ( !CreateResponseBase(&Request->RequestBase, rb, Response.HwId) ) return 0;

	Get16RandomBytes(Response.Rand);
	Sha256(Response.Rand, sizeof(Response.Rand), Response.Hash);
	memcpy(Response.XorSalts, Request->Salt, sizeof(Response.XorSalts));
	XorBlock(Response.XorSalts, Response.Rand);

	BYTE* current = response_data, *encrypt_start = response_data + V6_UNENCRYPTED_SIZE;
	size_t copySize = V6_PRE_EPID_SIZE + LE32(rb->KmsPIDLen);

	memcpy(response_data, &Response, copySize);
	current += copySize;

	copySize = MajorVer == 5 ? V5_POST_EPID_SIZE : V6_POST_EPID_SIZE;
	memcpy(current, &rb->ClientMachineId, copySize);
	current += copySize;

	size_t encryptSize = current - encrypt_start;

	if (MajorVer > 5 && !CreateV6Hmac(&Response, &aes, encrypt_start, encryptSize, current, 0)) return 0;

	// Padding auto handled by encryption func
	AesEncryptCbc(&aes, Response.Salt, encrypt_start, &encryptSize);

	return encryptSize + V6_UNENCRYPTED_SIZE;
}


static void EncryptRequestV6(REQUEST_V6* const Request)
{
	size_t encryptSize = sizeof(Request->RequestBase);
	AesCtx Ctx;
	BOOL v6 = LE16(Request->MajorVer) == 6;
	AesInitKey(&Ctx, v6 ? AesKeyV6 : AesKeyV5, v6, 16);
	AesEncryptCbc(&Ctx, Request->Salt, (BYTE*)(&Request->RequestBase), &encryptSize);
}


// Create Hashed KMS Client Request Data for KMS Protocol Version 4
BYTE *CreateRequestV4(size_t *size, const REQUEST* requestBase)
{
	*size = sizeof(REQUEST_V4);

	// Build a proper KMS client request data
	BYTE *request = (BYTE *)malloc(sizeof(REQUEST_V4));

	// Temporary Pointer for access to REQUEST_V4 structure
	REQUEST_V4 *request_v4 = (REQUEST_V4 *)request;

	// Set KMS Client Request Base
	memcpy(&request_v4->RequestBase, requestBase, sizeof(REQUEST));

	// Generate Hash Signature
	AesCmacV4(request, sizeof(REQUEST), request_v4->Hash);

	// Return Request Data
	return request;
}


// Create Encrypted KMS Client Request Data for KMS Protocol Version 6
BYTE *CreateRequestV6(size_t *size, const REQUEST* requestBase)
{
	*size = sizeof(REQUEST_V6);

	// Build a Proper KMS Client Request Data
	BYTE *request = (BYTE *) malloc(sizeof(REQUEST_V6));

	// Temporary Pointer for access to REQUEST_V5 structure
	REQUEST_V6 *request_v6 = (REQUEST_V6 *) request;

	// KMS Protocol Version
	request_v6->Version = requestBase->Version;

	// Initialize the IV
	Get16RandomBytes(request_v6->Salt);

	// Set KMS Client Request Base
	memcpy(&request_v6->RequestBase, requestBase, sizeof(REQUEST));

	// Encrypt KMS Client Request
	EncryptRequestV6(request_v6);

	// Return Proper Request Data
	return request;
}


static uint8_t CheckPidLength(const RESPONSE *const r)
{
	unsigned int i;

	if (LE32(r->KmsPIDLen) > (PID_BUFFER_SIZE << 1)) return FALSE;
	if (r->KmsPID[(LE32(r->KmsPIDLen) >> 1) - 1]) return FALSE;

	for (i = 0; i < (LE32(r->KmsPIDLen) >> 1) - 2; i++)
	{
		if (!r->KmsPID[i]) return FALSE;
	}

	return TRUE;
}


RESPONSE_RESULT DecryptResponseV4(RESPONSE_V4* Response_v4, const int responseSize, uint8_t* const response, const uint8_t* const request)
{
	int copySize =
		sizeof(Response_v4->ResponseBase.Version) +
		sizeof(Response_v4->ResponseBase.KmsPIDLen) +
		(LE32(((RESPONSE_V4*)response)->ResponseBase.KmsPIDLen) <= PID_BUFFER_SIZE << 1 ?
		LE32(((RESPONSE_V4*)response)->ResponseBase.KmsPIDLen) :
		PID_BUFFER_SIZE << 1);

	int messageSize =
		copySize +
		sizeof(Response_v4->ResponseBase.ClientMachineId) +
		sizeof(Response_v4->ResponseBase.TimeStamp) +
		sizeof(Response_v4->ResponseBase.ActivatedMachines) +
		sizeof(Response_v4->ResponseBase.ActivationInterval) +
		sizeof(Response_v4->ResponseBase.RenewalInterval);

	memcpy(Response_v4, response, copySize);
	memcpy(&Response_v4->ResponseBase.ClientMachineId, response + copySize, responseSize - copySize);

	// ensure PID is null terminated
	Response_v4->ResponseBase.KmsPID[PID_BUFFER_SIZE-1] = 0;

	uint8_t* hash = response + messageSize;
	AesCmacV4(response, messageSize, hash);

	REQUEST_V4* request_v4 = (REQUEST_V4*)request;
	RESPONSE_RESULT result;

	result.mask					 = (DWORD)-1;
	result.PidLengthOK			 = CheckPidLength((RESPONSE*)response);
	result.VersionOK			 = Response_v4->ResponseBase.Version == request_v4->RequestBase.Version;
	result.HashOK				 = !memcmp(&Response_v4->Hash, hash, sizeof(Response_v4->Hash));
	result.TimeStampOK			 = !memcmp(&Response_v4->ResponseBase.TimeStamp, &request_v4->RequestBase.TimeStamp, sizeof(FILETIME));
	result.ClientMachineIDOK	 = !memcmp(&Response_v4->ResponseBase.ClientMachineId, &request_v4->RequestBase.ClientMachineId, sizeof(GUID));
	result.effectiveResponseSize = responseSize;
	result.correctResponseSize	 = sizeof(RESPONSE_V4) - sizeof(Response_v4->ResponseBase.KmsPID) + LE32(Response_v4->ResponseBase.KmsPIDLen);

	return result;
}


RESPONSE_RESULT DecryptResponseV6(RESPONSE_V6* Response_v6, int responseSize, uint8_t* const response, const uint8_t* const request, BYTE* hwid)
{
	RESPONSE_RESULT result;
	result.mask = (DWORD) -1;
	result.effectiveResponseSize = responseSize;

	int copySize1 =
		sizeof(Response_v6->Version);

	// Decrypt KMS Server Response (encrypted part starts after RequestIV)
	responseSize -= copySize1;

	AesCtx Ctx;
	BOOL v6 = LE16(((RESPONSE_V6*)response)->MajorVer) > 5;

	AesInitKey(&Ctx, v6 ? AesKeyV6 : AesKeyV5, v6, AES_KEY_BYTES);
	AesDecryptCbc(&Ctx, NULL, response + copySize1, responseSize);

	// Check padding
	BYTE* lastPadByte = response + (size_t)result.effectiveResponseSize - 1;

	// Must be from 1 to 16
	if (!*lastPadByte || *lastPadByte > AES_BLOCK_BYTES)
	{
		result.DecryptSuccess = FALSE;
		return result;
	}
	else
	{
		// Check if pad bytes are all the same
		BYTE* padByte;
		for (padByte = lastPadByte - *lastPadByte + 1; padByte < lastPadByte; padByte++)
		if (*padByte != *lastPadByte)
		{
			result.DecryptSuccess = FALSE;
			return result;
		}
	}

	// Add size of Version, KmsPIDLen and variable size PID
	copySize1 +=
		sizeof(Response_v6->Salt) +
		sizeof(Response_v6->ResponseBase.Version) +
		sizeof(Response_v6->ResponseBase.KmsPIDLen) +
		(LE32(((RESPONSE_V6*) response)->ResponseBase.KmsPIDLen) <= PID_BUFFER_SIZE << 1 ?
		LE32(((RESPONSE_V6*) response)->ResponseBase.KmsPIDLen) :
		PID_BUFFER_SIZE << 1);

	// Copy part 1 of response up to variable sized PID
	memcpy(Response_v6, response, copySize1);

	// ensure PID is null terminated
	Response_v6->ResponseBase.KmsPID[PID_BUFFER_SIZE - 1] = 0;

	// Get size of the rest
	int copySize2 =
		sizeof(Response_v6->ResponseBase.ClientMachineId) +
		sizeof(Response_v6->ResponseBase.TimeStamp) +
		sizeof(Response_v6->ResponseBase.ActivatedMachines) +
		sizeof(Response_v6->ResponseBase.ActivationInterval) +
		sizeof(Response_v6->ResponseBase.RenewalInterval) +
		sizeof(Response_v6->Rand) +
		sizeof(Response_v6->Hash);

	if (v6)
	{
		copySize2 +=
		sizeof(Response_v6->HwId) +
		sizeof(Response_v6->XorSalts) +
		sizeof(Response_v6->Hmac);
	}

	// Copy part 2
	memcpy(&Response_v6->ResponseBase.ClientMachineId, response + copySize1, copySize2);

	// Decrypting the response is finished here. Now we check the results for validity

	REQUEST_V6* request_v6 = (REQUEST_V6*) request;
	DWORD decryptSize = sizeof(request_v6->Salt) + sizeof(request_v6->RequestBase) + sizeof(request_v6->Pad);

	AesDecryptCbc(&Ctx, NULL, request_v6->Salt, decryptSize);

	// Check that all version informations are the same
	result.VersionOK =
		request_v6->Version == Response_v6->ResponseBase.Version &&
		request_v6->Version == Response_v6->Version &&
		request_v6->Version == request_v6->RequestBase.Version;

	// Check Base Request
	result.PidLengthOK			= CheckPidLength(&((RESPONSE_V6*) response)->ResponseBase);
	result.TimeStampOK			= !memcmp(&Response_v6->ResponseBase.TimeStamp, &request_v6->RequestBase.TimeStamp, sizeof(FILETIME));
	result.ClientMachineIDOK	= IsEqualGUID(&Response_v6->ResponseBase.ClientMachineId, &request_v6->RequestBase.ClientMachineId);

	// Rebuild Random Key and Sha256 Hash
	BYTE HashVerify[sizeof(Response_v6->Hash)];
	BYTE RandomKey[sizeof(Response_v6->Rand)];

	memcpy(RandomKey, request_v6->Salt, sizeof(RandomKey));
	XorBlock(Response_v6->Rand, RandomKey);
	Sha256(RandomKey, sizeof(RandomKey), HashVerify);

	result.HashOK = !memcmp(Response_v6->Hash, HashVerify, sizeof(HashVerify));

	// Version specific stuff
	if (v6)
	{
		// Copy the HwId
		memcpy(hwid, Response_v6->HwId, sizeof(Response_v6->HwId));

		// Check IVs
		result.IVsOK = !memcmp(Response_v6->XorSalts, request_v6->Salt, sizeof(Response_v6->XorSalts));

		// size before encryption
		result.correctResponseSize  = sizeof(RESPONSE_V6) - sizeof(Response_v6->ResponseBase.KmsPID) + LE32(Response_v6->ResponseBase.KmsPIDLen);

		// Check Hmac
		int_fast8_t tolerance;
		BYTE OldHmac[sizeof(Response_v6->Hmac)];

		result.HmacSha256OK = FALSE;
		memcpy(OldHmac, Response_v6->Hmac, sizeof(Response_v6->Hmac));
		AesEncryptBlock(&Ctx, Response_v6->Salt);

		for (tolerance = -1; tolerance < 2; tolerance++)
		{
			CreateV6Hmac(
					Response_v6,
					&Ctx,
					response + sizeof(Response_v6->Version) + sizeof(Response_v6->Salt),
					(size_t)result.correctResponseSize - sizeof(Response_v6->Version) - sizeof(Response_v6->Salt),
					response + (size_t)result.correctResponseSize,
					tolerance
			);

			if ((result.HmacSha256OK = !memcmp(OldHmac, response + (size_t)result.correctResponseSize - sizeof(Response_v6->Hmac), sizeof(OldHmac))))
				break;
		}
	}
	else // V5
	{
		// Check IVs
		result.IVsOK = !memcmp(request_v6->Salt, Response_v6->Salt, sizeof(request_v6->Salt));

		// V5 has no Hmac
		result.HmacSha256OK = TRUE;

		// size before encryption
		result.correctResponseSize = sizeof(RESPONSE_V5) - sizeof(Response_v6->ResponseBase.KmsPID) + LE32(Response_v6->ResponseBase.KmsPIDLen);
	}

	// padded size after encryption
	result.correctResponseSize += (~(result.correctResponseSize - sizeof(Response_v6->ResponseBase.Version)) & 0xf) + 1;

	return result;
}
