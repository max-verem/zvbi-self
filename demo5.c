#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#include "demo5-hamming.h"
#include "demo5-teletext.h"


typedef enum {
	NO = 0x00,
	YES = 0x01,
	UNDEF = 0xff
} bool_t;


typedef enum {
	DATA_UNIT_EBU_TELETEXT_NONSUBTITLE = 0x02,
	DATA_UNIT_EBU_TELETEXT_SUBTITLE = 0x03,
	DATA_UNIT_EBU_TELETEXT_INVERTED = 0x0c,
	DATA_UNIT_VPS = 0xc3,
	DATA_UNIT_CLOSED_CAPTIONS = 0xc5
} data_unit_t;

typedef enum {
	TRANSMISSION_MODE_PARALLEL = 0,
	TRANSMISSION_MODE_SERIAL = 1
} transmission_mode_t;


// 1-byte alignment; just to be sure, this struct is being used for explicit type conversion
// FIXME: remove explicit type conversion from buffer to structs
#pragma pack(push)
#pragma pack(1)
typedef struct {
	uint8_t _clock_in; // clock run in
	uint8_t _framing_code; // framing code, not needed, ETSI 300 706: const 0xe4
	uint8_t address[2];
	uint8_t data[40];
} teletext_packet_payload_t;
#pragma pack(pop)

typedef struct {
	uint64_t show_timestamp; // show at timestamp (in ms)
	uint64_t hide_timestamp; // hide at timestamp (in ms)
	uint16_t text[25][40]; // 25 lines x 40 cols (1 screen/page) of wide chars
	uint8_t tainted; // 1 = text variable contains any data
} teletext_page_t;

typedef struct {
	uint64_t show_timestamp; // show at timestamp (in ms)
	uint64_t hide_timestamp; // hide at timestamp (in ms)
	char *text;
} frame_t;


// subtitle type pages bitmap, 2048 bits = 2048 possible pages in teletext (excl. subpages)
static uint8_t cc_map[256] = { 0 };

// teletext transmission mode
static transmission_mode_t transmission_mode = TRANSMISSION_MODE_SERIAL;

// flag indicating if incoming data should be processed or ignored
static uint8_t receiving_data = NO;

// working teletext page buffer
static teletext_page_t page_buffer = { 0 };

// last timestamp computed
uint64_t last_timestamp = 0;

// current charset (charset can be -- and always is -- changed during transmission)
static struct {
	uint8_t current;
	uint8_t g0_m29;
	uint8_t g0_x28;
} primary_charset = {
	.current = 0x00,
	.g0_m29 = UNDEF,
	.g0_x28 = UNDEF
};


// application config global variable
static struct {
#ifdef __MINGW32__
	wchar_t *input_name; // input file name (used on Windows, UNICODE)
	wchar_t *output_name; // output file name (used on Windows, UNICODE)
#else
	char *input_name; // input file name
	char *output_name; // output file name
#endif
	uint8_t verbose; // should telxcc be verbose?
	uint16_t page; // teletext page containing cc we want to filter
	uint16_t tid; // 13-bit packet ID for teletext stream
	double offset; // time offset in seconds
	uint8_t colours; // output <font...></font> tags
	uint8_t bom; // print UTF-8 BOM characters at the beginning of output
	uint8_t nonempty; // produce at least one (dummy) frame
	uint64_t utc_refvalue; // UTC referential value
	// FIXME: move SE_MODE to output module
	uint8_t se_mode;
	//char *template; // output format template
	uint8_t m2ts; // consider input stream is af s M2TS, instead of TS
} config = {
	.input_name = NULL,
	.output_name = NULL,
	.verbose = YES /*NO*/,
	.page = 0,
	.tid = 0,
	.offset = 0,
	.colours = NO,
	.bom = YES,
	.nonempty = NO,
	.utc_refvalue = 0,
	.se_mode = NO,
	//.template = NULL,
	.m2ts = NO
};



// application states -- flags for notices that should be printed only once
static struct {
	uint8_t programme_info_processed;
	uint8_t pts_initialized;
} states = {
	.programme_info_processed = NO,
	.pts_initialized = NO
};


// macro -- output only when increased verbosity was turned on
#define VERBOSE_ONLY if (config.verbose == YES)


static void remap_g0_charset(uint8_t c) {
	if (c != primary_charset.current) {
		uint8_t m = G0_LATIN_NATIONAL_SUBSETS_MAP[c];
		if (m == 0xff) {
			fprintf(stderr, "- G0 Latin National Subset ID 0x%1x.%1x is not implemented\n", (c >> 3), (c & 0x7));
		}
		else {
			for (uint8_t j = 0; j < 13; j++) G0[LATIN][G0_LATIN_NATIONAL_SUBSETS_POSITIONS[j]] = G0_LATIN_NATIONAL_SUBSETS[m].characters[j];
			VERBOSE_ONLY fprintf(stderr, "- Using G0 Latin National Subset ID 0x%1x.%1x (%s)\n", (c >> 3), (c & 0x7), G0_LATIN_NATIONAL_SUBSETS[m].language);
			primary_charset.current = c;
		}
	}
}

// UCS-2 (16 bits) to UTF-8 (Unicode Normalization Form C (NFC)) conversion
static void ucs2_to_utf8(char *r, uint16_t ch) {
	if (ch < 0x80) {
		r[0] = ch & 0x7f;
		r[1] = 0;
		r[2] = 0;
		r[3] = 0;
	}
	else if (ch < 0x800) {
		r[0] = (ch >> 6) | 0xc0;
		r[1] = (ch & 0x3f) | 0x80;
		r[2] = 0;
		r[3] = 0;
	}
	else {
		r[0] = (ch >> 12) | 0xe0;
		r[1] = ((ch >> 6) & 0x3f) | 0x80;
		r[2] = (ch & 0x3f) | 0x80;
		r[3] = 0;
	}
}

// check parity and translate any reasonable teletext character into ucs2
static uint16_t telx_to_ucs2(uint8_t c) {
	if (PARITY_8[c] == 0) {
		VERBOSE_ONLY fprintf(stderr, "! Unrecoverable data error; PARITY(%02x)\n", c);
		return 0x20;
	}

	uint16_t r = c & 0x7f;
	if (r >= 0x20) r = G0[LATIN][r - 0x20];
	return r;
}




// extracts magazine number from teletext page
#define MAGAZINE(p) ((p >> 8) & 0xf)

// extracts page number from teletext page
#define PAGE(p) (p & 0xff)

// ETS 300 706, chapter 8.2
static uint8_t unham_8_4(uint8_t a) {
	uint8_t r = UNHAM_8_4[a];
	if (r == 0xff) {
		r = 0;
		VERBOSE_ONLY fprintf(stderr, "! Unrecoverable data error; UNHAM8/4(%02x)\n", a);
	}
	return (r & 0x0f);
}

// ETS 300 706, chapter 8.3
static uint32_t unham_24_18(uint32_t a) {
	uint8_t test = 0;

	// Tests A-F correspond to bits 0-6 respectively in 'test'.
	for (uint8_t i = 0; i < 23; i++) test ^= ((a >> i) & 0x01) * (i + 33);
	// Only parity bit is tested for bit 24
	test ^= ((a >> 23) & 0x01) * 32;

	if ((test & 0x1f) != 0x1f) {
		// Not all tests A-E correct
		if ((test & 0x20) == 0x20) {
			// F correct: Double error
			return 0xffffffff;
		}
		// Test F incorrect: Single error
		a ^= 1 << (30 - test);
	}

	return (a & 0x000004) >> 2 | (a & 0x000070) >> 3 | (a & 0x007f00) >> 4 | (a & 0x7f0000) >> 5;
}


static void process_page(teletext_page_t *page) {
    int i, j;

    fprintf(stderr, "%s:%d: HERE\n", __FUNCTION__, __LINE__);

    for(i = 0; i < 40; i++)
        wprintf(L"-");
    wprintf(L"\n");

    for(j = 0; j < 25; j++)
    {
        for(i = 0; i < 40; i++)
            wprintf(L"%c", (page->text[j][i]) ? (page->text[j][i]) : ' ');
        wprintf(L"\n");
    };
};

static void process_telx_packet(data_unit_t data_unit_id, teletext_packet_payload_t *packet, uint64_t timestamp) {
	// variable names conform to ETS 300 706, chapter 7.1.2
	uint8_t address = (unham_8_4(packet->address[1]) << 4) | unham_8_4(packet->address[0]);
	uint8_t m = address & 0x7;
	if (m == 0) m = 8;
	uint8_t y = (address >> 3) & 0x1f;
	uint8_t designation_code = (y > 25) ? unham_8_4(packet->data[0]) : 0x00;


fprintf(stderr, "%s:%d: m=%d, y=%d\n", __FUNCTION__, __LINE__, (int)m, (int)y);


	if (y == 0) {
		// CC map
		uint8_t i = (unham_8_4(packet->data[1]) << 4) | unham_8_4(packet->data[0]);
		uint8_t flag_subtitle = (unham_8_4(packet->data[5]) & 0x08) >> 3;
		cc_map[i] |= flag_subtitle << (m - 1);

fprintf(stderr, "%s:%d: if (y == 0) {\n", __FUNCTION__, __LINE__);

		if ((config.page == 0) && (flag_subtitle == YES) && (i < 0xff)) {
			config.page = (m << 8) | (unham_8_4(packet->data[1]) << 4) | unham_8_4(packet->data[0]);
			fprintf(stderr, "- No teletext page specified, first received suitable page is %03x, not guaranteed\n", config.page);
		}

	 	// Page number and control bits
		uint16_t page_number = (m << 8) | (unham_8_4(packet->data[1]) << 4) | unham_8_4(packet->data[0]);
		uint8_t charset = ((unham_8_4(packet->data[7]) & 0x08) | (unham_8_4(packet->data[7]) & 0x04) | (unham_8_4(packet->data[7]) & 0x02)) >> 1;
		//uint8_t flag_suppress_header = unham_8_4(packet->data[6]) & 0x01;
		//uint8_t flag_inhibit_display = (unham_8_4(packet->data[6]) & 0x08) >> 3;


fprintf(stderr, "%s:%d: page_number=%d (0x%X), charset=%d (0x%X)\n",
__FUNCTION__, __LINE__,
page_number, page_number,
charset, charset);

		// ETS 300 706, chapter 9.3.1.3:
		// When set to '1' the service is designated to be in Serial mode and the transmission of a page is terminated
		// by the next page header with a different page number.
		// When set to '0' the service is designated to be in Parallel mode and the transmission of a page is terminated
		// by the next page header with a different page number but the same magazine number.
		// The same setting shall be used for all page headers in the service.
		// ETS 300 706, chapter 7.2.1: Page is terminated by and excludes the next page header packet
		// having the same magazine address in parallel transmission mode, or any magazine address in serial transmission mode.
		transmission_mode = unham_8_4(packet->data[7]) & 0x01;

		// FIXME: Well, this is not ETS 300 706 kosher, however we are interested in DATA_UNIT_EBU_TELETEXT_SUBTITLE only
		if ((transmission_mode == TRANSMISSION_MODE_PARALLEL) && (data_unit_id != DATA_UNIT_EBU_TELETEXT_SUBTITLE)) return;

		if ((receiving_data == YES) && (
				((transmission_mode == TRANSMISSION_MODE_SERIAL) && (PAGE(page_number) != PAGE(config.page))) ||
				((transmission_mode == TRANSMISSION_MODE_PARALLEL) && (PAGE(page_number) != PAGE(config.page)) && (m == MAGAZINE(config.page)))
			)) {
			receiving_data = NO;
			return;
		}

		// Page transmission is terminated, however now we are waiting for our new page
		if (page_number != config.page) return;

		// Now we have the begining of page transmission; if there is page_buffer pending, process it
		if (page_buffer.tainted == YES) {
			// it would be nice, if subtitle hides on previous video frame, so we contract 40 ms (1 frame @25 fps)
			page_buffer.hide_timestamp = timestamp - 40;
			process_page(&page_buffer);
		}

		page_buffer.show_timestamp = timestamp;
		page_buffer.hide_timestamp = 0;
		memset(page_buffer.text, 0x00, sizeof(page_buffer.text));
		page_buffer.tainted = NO;
		receiving_data = YES;
		primary_charset.g0_x28 = UNDEF;

		uint8_t c = (primary_charset.g0_m29 != UNDEF) ? primary_charset.g0_m29 : charset;
		remap_g0_charset(c);

		/*
		// I know -- not needed; in subtitles we will never need disturbing teletext page status bar
		// displaying tv station name, current time etc.
		if (flag_suppress_header == NO) {
			for (uint8_t i = 14; i < 40; i++) page_buffer.text[y][i] = telx_to_ucs2(packet->data[i]);
			//page_buffer.tainted = YES;
		}
		*/
	}
	else if ((m == MAGAZINE(config.page)) && (y >= 1) && (y <= 23) && (receiving_data == YES)) {

fprintf(stderr, "%s:%d: else if ((m == MAGAZINE(config.page)) && (y >= 1) && (y <= 23) && (receiving_data == YES)) {\n",
__FUNCTION__, __LINE__);

		// ETS 300 706, chapter 9.4.1: Packets X/26 at presentation Levels 1.5, 2.5, 3.5 are used for addressing
		// a character location and overwriting the existing character defined on the Level 1 page
		// ETS 300 706, annex B.2.2: Packets with Y = 26 shall be transmitted before any packets with Y = 1 to Y = 25;
		// so page_buffer.text[y][i] may already contain any character received
		// in frame number 26, skip original G0 character
		for (uint8_t i = 0; i < 40; i++) if (page_buffer.text[y][i] == 0x00) page_buffer.text[y][i] = telx_to_ucs2(packet->data[i]);
		page_buffer.tainted = YES;

fprintf(stderr, "%s:%d: packet->data=[%*s]\n", __FUNCTION__, __LINE__,  40, packet->data);

	}
	else if ((m == MAGAZINE(config.page)) && (y == 26) && (receiving_data == YES)) {

		// ETS 300 706, chapter 12.3.2: X/26 definition
		uint8_t x26_row = 0;
		uint8_t x26_col = 0;

		uint32_t triplets[13] = { 0 };

fprintf(stderr, "%s:%d: else if ((m == MAGAZINE(config.page)) && (y == 26) && (receiving_data == YES)) {\n", __FUNCTION__, __LINE__);

		for (uint8_t i = 1, j = 0; i < 40; i += 3, j++) triplets[j] = unham_24_18((packet->data[i + 2] << 16) | (packet->data[i + 1] << 8) | packet->data[i]);

		for (uint8_t j = 0; j < 13; j++) {
			if (triplets[j] == 0xffffffff) {
				// invalid data (HAM24/18 uncorrectable error detected), skip group
				VERBOSE_ONLY fprintf(stderr, "! Unrecoverable data error; UNHAM24/18()=%04x\n", triplets[j]);
				continue;
			}

			uint8_t data = (triplets[j] & 0x3f800) >> 11;
			uint8_t mode = (triplets[j] & 0x7c0) >> 6;
			uint8_t address = triplets[j] & 0x3f;
			uint8_t row_address_group = (address >= 40) && (address <= 63);

			// ETS 300 706, chapter 12.3.1, table 27: set active position
			if ((mode == 0x04) && (row_address_group == YES)) {
				x26_row = address - 40;
				if (x26_row == 0) x26_row = 24;
				x26_col = 0;
			}

			// ETS 300 706, chapter 12.3.1, table 27: termination marker
			if ((mode >= 0x11) && (mode <= 0x1f) && (row_address_group == YES)) break;

			// ETS 300 706, chapter 12.3.1, table 27: character from G2 set
			if ((mode == 0x0f) && (row_address_group == NO)) {
				x26_col = address;
				if (data > 31) page_buffer.text[x26_row][x26_col] = G2[0][data - 0x20];
			}

			// ETS 300 706, chapter 12.3.1, table 27: G0 character with diacritical mark
			if ((mode >= 0x11) && (mode <= 0x1f) && (row_address_group == NO)) {
				x26_col = address;

				// A - Z
				if ((data >= 65) && (data <= 90)) page_buffer.text[x26_row][x26_col] = G2_ACCENTS[mode - 0x11][data - 65];
				// a - z
				else if ((data >= 97) && (data <= 122)) page_buffer.text[x26_row][x26_col] = G2_ACCENTS[mode - 0x11][data - 71];
				// other
				else page_buffer.text[x26_row][x26_col] = telx_to_ucs2(data);
			}
		}
	}
	else if ((m == MAGAZINE(config.page)) && (y == 28) && (receiving_data == YES)) {

fprintf(stderr, "%s:%d: else if ((m == MAGAZINE(config.page)) && (y == 28) && (receiving_data == YES)) {\n", __FUNCTION__, __LINE__);

		// TODO:
		//   ETS 300 706, chapter 9.4.7: Packet X/28/4
		//   Where packets 28/0 and 28/4 are both transmitted as part of a page, packet 28/0 takes precedence over 28/4 for all but the colour map entry coding.
		if ((designation_code == 0) || (designation_code == 4)) {
			// ETS 300 706, chapter 9.4.2: Packet X/28/0 Format 1
			// ETS 300 706, chapter 9.4.7: Packet X/28/4
			uint32_t triplet0 = unham_24_18((packet->data[3] << 16) | (packet->data[2] << 8) | packet->data[1]);

			if (triplet0 == 0xffffffff) {
				// invalid data (HAM24/18 uncorrectable error detected), skip group
				VERBOSE_ONLY fprintf(stderr, "! Unrecoverable data error; UNHAM24/18()=%04x\n", triplet0);
			}
			else {
				// ETS 300 706, chapter 9.4.2: Packet X/28/0 Format 1 only
				if ((triplet0 & 0x0f) == 0x00) {
					primary_charset.g0_x28 = (triplet0 & 0x3f80) >> 7;
					remap_g0_charset(primary_charset.g0_x28);
				}
			}
		}
	}
	else if ((m == MAGAZINE(config.page)) && (y == 29)) {

fprintf(stderr, "%s:%d: else if ((m == MAGAZINE(config.page)) && (y == 29)) {\n", __FUNCTION__, __LINE__);

		// TODO:
		//   ETS 300 706, chapter 9.5.1 Packet M/29/0
		//   Where M/29/0 and M/29/4 are transmitted for the same magazine, M/29/0 takes precedence over M/29/4.
		if ((designation_code == 0) || (designation_code == 4)) {
			// ETS 300 706, chapter 9.5.1: Packet M/29/0
			// ETS 300 706, chapter 9.5.3: Packet M/29/4
			uint32_t triplet0 = unham_24_18((packet->data[3] << 16) | (packet->data[2] << 8) | packet->data[1]);

			if (triplet0 == 0xffffffff) {
				// invalid data (HAM24/18 uncorrectable error detected), skip group
				VERBOSE_ONLY fprintf(stderr, "! Unrecoverable data error; UNHAM24/18()=%04x\n", triplet0);
			}
			else {
				// ETS 300 706, table 11: Coding of Packet M/29/0
				// ETS 300 706, table 13: Coding of Packet M/29/4
				if ((triplet0 & 0xff) == 0x00) {
					primary_charset.g0_m29 = (triplet0 & 0x3f80) >> 7;
					// X/28 takes precedence over M/29
					if (primary_charset.g0_x28 == UNDEF) {
						remap_g0_charset(primary_charset.g0_m29);
					}
				}
			}
		}
	}
	else if ((m == 8) && (y == 30)) {

fprintf(stderr, "%s:%d: else if ((m == 8) && (y == 30)) {\n", __FUNCTION__, __LINE__);

		// ETS 300 706, chapter 9.8: Broadcast Service Data Packets
		if (states.programme_info_processed == NO) {
			// ETS 300 706, chapter 9.8.1: Packet 8/30 Format 1
			if (unham_8_4(packet->data[0]) < 2) {
				fprintf(stderr, "- Programme Identification Data = ");
				for (uint8_t i = 20; i < 40; i++) {
					uint8_t c = telx_to_ucs2(packet->data[i]);
					// strip any control codes from PID, eg. TVP station
					if (c < 0x20) continue;

					char u[4] = { 0, 0, 0, 0 };
					ucs2_to_utf8(u, c);
					fprintf(stderr, "%s", u);
				}
				fprintf(stderr, "\n");

				// OMG! ETS 300 706 stores timestamp in 7 bytes in Modified Julian Day in BCD format + HH:MM:SS in BCD format
				// + timezone as 5-bit count of half-hours from GMT with 1-bit sign
				// In addition all decimals are incremented by 1 before transmission.
				uint32_t t = 0;
				// 1st step: BCD to Modified Julian Day
				t += (packet->data[10] & 0x0f) * 10000;
				t += ((packet->data[11] & 0xf0) >> 4) * 1000;
				t += (packet->data[11] & 0x0f) * 100;
				t += ((packet->data[12] & 0xf0) >> 4) * 10;
				t += (packet->data[12] & 0x0f);
				t -= 11111;
				// 2nd step: conversion Modified Julian Day to unix timestamp
				t = (t - 40587) * 86400;
				// 3rd step: add time
				t += 3600 * ( ((packet->data[13] & 0xf0) >> 4) * 10 + (packet->data[13] & 0x0f) );
				t +=   60 * ( ((packet->data[14] & 0xf0) >> 4) * 10 + (packet->data[14] & 0x0f) );
				t +=        ( ((packet->data[15] & 0xf0) >> 4) * 10 + (packet->data[15] & 0x0f) );
				t -= 40271;
				// 4th step: conversion to time_t
				time_t t0 = (time_t)t;
				// ctime output itself is \n-ended
				fprintf(stderr, "- Programme Timestamp (UTC) = %s", ctime(&t0));

				VERBOSE_ONLY fprintf(stderr, "- Transmission mode = %s\n", (transmission_mode == TRANSMISSION_MODE_SERIAL ? "serial" : "parallel"));

				if (config.se_mode == YES) {
					fprintf(stderr, "- Broadcast Service Data Packet received, resetting UTC referential value to %s", ctime(&t0));
					config.utc_refvalue = t;
					states.pts_initialized = NO;
				}

				states.programme_info_processed = YES;
			}
		}
	}
}


#define BS 42

int main(int argc, char** argv)
{
    FILE* f;
    int r, skip = 0, cnt = 0;

    if(argc < 2)
    {
        fprintf(stderr, "Please specify sliced vbi data file\n");
        return 1;
    };

    if(!strcmp("-", argv[1]))
        f = stdin;
    else
        f = fopen(argv[1], "rb");
    if(!f)
    {
        fprintf(stderr, "Failed to open [%s]\n", argv[1]);
        return 1;
    };

    if(argc > 2)
    {
        skip = atoi(argv[2]);
    };

    while(!feof(f))
    {
        int l = skip + BS;
        unsigned char temp[1024];

        memset(temp, 0, sizeof(temp));
        l = fread(temp, 1, skip + BS, f);

        if(l == (skip + BS))
        {
            teletext_packet_payload_t py;

            fprintf(stderr, "\n%s:%d: sending packet %5d\n", __FUNCTION__, __LINE__, cnt++);

            memset(&py, 0, sizeof(py));
            memcpy(py.address,  temp + skip + 0, 2);
            memcpy(py.data,     temp + skip + 2, 40);

            process_telx_packet(
#if 0
                DATA_UNIT_EBU_TELETEXT_NONSUBTITLE
#else
                DATA_UNIT_EBU_TELETEXT_SUBTITLE
#endif
                ,&py,
                last_timestamp);

            last_timestamp += 10000;
        }
        else if (l)
            fprintf(stderr, "%s:%d: fread(stdin) %d != %d\n", __FUNCTION__, __LINE__, skip + BS, l);
    };

    fclose(f);

    return 0;
};
