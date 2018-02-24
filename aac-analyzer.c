#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s aac-file", argv[0]);
	}

	scan_aac(argv[1]);

	return 1;
}


void error(const char *msg) 
{
	fprintf(stderr, msg);
	exit(1);
}

void say(const char *msg)
{
	printf(msg);
	printf("\n");
}

int scan_aac(const char *file) 
{
	FILE *fp	= fopen(file, "r");
	if (!fp) {
		fprintf(stderr, "can't open file");
		exit(1);
	}
	char buf[200000];
	int header_len  = 7; // headerは7byte
	while (! feof(fp)) {
		int aac_frame_len   = crawl_aac(fp);

		int readed  = fread(buf, 1, aac_frame_len - header_len, fp);
		if (readed != aac_frame_len - header_len) {
			printf("expected : %d, readed: %d\n", aac_frame_len - header_len, readed);
			exit(0);
		}
		say("----------------------------");
	}
}

int get_byte(FILE *fp) {
	int c   = fgetc(fp);
	if (c == EOF) {
		say("FILE END");
		exit(1);
	}
	return c;
}

int crawl_aac(FILE *fp) 
{

	// スキャン開始
	// ID3タグを読む(有れば)
	read_id3(fp);
	//int c;
	unsigned char c, c2;
	c	= get_byte(fp);
	if (! (c & 0xff)) {
		error("AACではないです。");
	}

	c	= get_byte(fp);
	if (! (c & 0xf0)) { 
		error("AACではないです。");
	}

	// check ID
	switch ((c & 0x08) >> 3) {
		case 0:
		say("MPEG-4");  break;
		case 1:
		say("MPEG-2");  break;
	}

	// protection 1:CRC保護なし、0:CRC保護有り
	switch (c & 0x01) {
		case 1:
		say("CRC保護なし");  break;
		case 0:
		say("CRC保護有り");  break;
	}

    // byte 3
	c	= get_byte(fp);
	
	// profile
	switch ((c & 0xc0) >> 6) {
		case 0:
		say("MAIN");  break;
		case 1:
		say("LC");  break;
		case 2:
		say("SSR");  break;
		case 3:
		say("(reserved)");  break;
	}
	// sampling frequency
	int sampling_frequency	= (c & 0x3c) >> 2;
	printf("sampl %d\n", sampling_frequency);
	printf("0x%x\n", c);
	switch ((c & 0x3c) >> 2) {
		case 0:
		say ("96000 Hz"); break;
		case 1:
		say ("88200 Hz"); break;
		case 2:
		say ("64000 Hz"); break;
		case 3:
		say ("48000 Hz"); break;
		case 4:
		say ("44100 Hz"); break;
		case 5:
		say ("32000 Hz"); break;
		case 6:
		say ("24000 Hz"); break;
		case 7:
		say ("22050 Hz"); break;
		case 8:
		say ("16000 Hz"); break;
		case 9:
		say ("12000 Hz"); break;
		case 10:
		say ("11025 Hz"); break;
		case 11:
		say ("8000 Hz"); break;
	}
	// private_bit
	int private_bit	= (0x02 & c) >> 1;
	printf("private_bit = 0x%x\n", private_bit);
	int channel	= 0x01 & c;
	c	= get_byte(fp);
	channel = (channel << 2) | ((c & 0xc0) >> 6);
	// channel
	printf("%d channel\n", channel);
	
	// original/copy
	// ISO/IEC 13818-7 では、
	//   original_copy see ISO/IEC 11172-3, definition of data element copyright.
	// とあり、
	// ISO/IEC 11172-3 のcopyrightには、
	//   copyright - if this bit equals '0' there is no copyright on the coded bitstream, '1' means copyright 
	// とある。
	switch ((c & 0x20) >> 5) {
		case 0:
		say ("no copyright"); break;
		case 1:
		say ("copyright"); break;
	}

	// home (0 = encoding, 1 = decoding)
	// ISO/IEC 13818-7 では、
	//   home see ISO/IEC 11172-3, definition of data element original/copy
	// とあり(original/homeの間違い？)、
	// ISO/IEC 11172-3 のoriginal/homeには、
	//   original/home - this bit equals '0' if the bitstream is a copy, '1' if it is an original 
	// とある。
	switch ((c & 0x10) >> 4) {
		case 0:
			say ("Home 0(copy)"); break;
		case 1:
			say ("Home 1(original)"); break;
	}

	// aac frame length ( 4byte目の下位2bit ～ 6byte目の上位3bitまで )
	unsigned int aac_frame_length	= 0x03 & c;
	c	= get_byte(fp);
	aac_frame_length	= (aac_frame_length << 8) | c;
	c	= get_byte(fp);
	aac_frame_length	= (aac_frame_length << 3) | ((c & 0xe0) >> 5);
	printf("aac frame length : %dbyte\n", aac_frame_length);

	// adts buffer fullness バッファー占有量
	unsigned int buffer_fullness	= 0x1f & c;
	c	= get_byte(fp);
	buffer_fullness		= (buffer_fullness << 6) | ((c & 0xfc) >> 2);
	if (buffer_fullness == 0x7ff) {
		printf("adts buffer fullness : VBR\n");
	}
	else {
		printf("adts buffer fullness : %d (0x%x)\n", buffer_fullness, buffer_fullness);
	}

	// adtsフレーム内のAACフレームの数
	unsigned int aac_frame_count	= (0x03 & c) -1;
	printf("aac frame count : %d\n", aac_frame_count);

	return aac_frame_length;
}

int read_id3(FILE *fp) 
{
	unsigned char id3[3];
	id3[0]  = get_byte(fp);
	id3[1]  = get_byte(fp);
	id3[2]  = get_byte(fp);

	int header_size = -1;
	if (strncmp(id3, "ID3", 3) == 0) {
		printf(" HAS ID3 TAG \n");
		// ID3
		// 3byte捨てる
		unsigned char size[4];
		fread(size, 1, 3, fp);
		header_size	= read_frame_size(fp);
		printf("size : %d\n", header_size);
		// データを読む
		header_size	-= 10;
		while (header_size > 0) {
			printf("reman header %dbyes\n", header_size);
			int readed	= read_id3_frame(fp);
			header_size	-= readed;
		}
	}
	else {
		ungetc(id3[2], fp);
		ungetc(id3[1], fp);
		ungetc(id3[0], fp);
	}
}

// return : 読み込んだバイト数
int read_id3_frame(FILE *fp) {
	unsigned char id[5]	= {0};
	fread(id, 1, 4, fp);
	int size	= read_frame_size(fp);
	printf(" ID3 Frame => %s, size = %d\n", id, size);

	// flagはスキップ(2bytes)
	fgetc(fp);
	fgetc(fp);

	// データ
	// owner identifier 00 data の形式のみサポートする
	if (size > 256) {
		fprintf(stderr, "too large frame data");
		exit(1);
	}
	unsigned char owner_id[256]	= {0};
	unsigned char priv_data[256]	= {0};
	int priv_data_idx	= 0;
	int is_reading_owner_id	= 1;
	int read_bytes	= size;
	while (read_bytes--) {
		unsigned char ch	= fgetc(fp);
		if (is_reading_owner_id == 1 && ch == 0) {
			is_reading_owner_id	= 0;
			continue;
		}

		if (is_reading_owner_id) {
			owner_id[strlen(owner_id)]	= ch;
		}
		else {
			priv_data[priv_data_idx++]	= ch;
		}
	}

	printf("OWNER:%s, DATA:", owner_id);
	int i	= 0;
	while (i != priv_data_idx) {
		printf("0x%x ", priv_data[i++]);
	}
	printf("\n");
	show_priv_data(priv_data, priv_data_idx);
	return size;
}

// PRIVデータのデータ部を数値化する。 for ts chunk
// upper 31bit must 0, and lower 33bit is data
void show_priv_data(const unsigned char *data, int size) {
	unsigned long long int count	= 0;
	int i;
	for (i = 3 ; i < size ; i++) {
		count	= (count << 8) + data[i];
	}
	printf("      count = %lld\n", count);
	// 90kHzで割る
	float msec	= (count % 90000) / 90000.0 * 1000;
	int sec	= (int)(count / 90000.0);
	printf("        sec = %d\n", sec);
	int min = (int)(sec / 60);
	sec		= sec % 60;
	int hour	= (int)(min / 60);
	min		= min % 60;
	printf("%d hour %d minutes %d second %.3f millisecond\n", hour, min, sec, msec);
}

// return : サイズ(bytes)
int read_frame_size(FILE *fp) {
	unsigned char size[4];
	// 4byte = header size (lower 7bit is acceptable)
	fread(size, 1, 4, fp);
	int frame_size =  (((uint8_t)size[0])<<21) + (((uint8_t)size[1])<<14) + (((uint8_t)size[2])<<7) + (uint8_t)size[3];
	return frame_size;
}

