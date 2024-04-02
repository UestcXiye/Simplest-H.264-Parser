/**
* ��򵥵� H.264 ��Ƶ������������
* Simplest H.264 Parser
*
* ԭ����
* ������ Lei Xiaohua
* leixiaohua1020@126.com
* �й���ý��ѧ/���ֵ��Ӽ���
* Communication University of China / Digital TV Technology
* http://blog.csdn.net/leixiaohua1020
*
* �޸ģ�
* ���ĳ� Liu Wenchen
* 812288728@qq.com
* ���ӿƼ���ѧ/������Ϣ
* University of Electronic Science and Technology of China / Electronic and Information Science
* https://blog.csdn.net/ProgramNovice
*
* ����Ŀ��һ�� H.264 �����������򣬿��Է��벢���� NALU��
*
* This project is a H.264 stream analysis program.
* It can parse H.264 bitstream and analysis NALU of stream.
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// �������fopen() ��������ȫ
#pragma warning(disable:4996)

// NALU ����ȡֵ����Ӧ NAL Header �� nal_unit_type��ռ 5 bit
typedef enum {
	NALU_TYPE_SLICE = 1, // �� IDR ֡�в��������ݻ��ֵ�Ƭ
	NALU_TYPE_DPA = 2, // �� IDR ֡�� A �����ݻ���Ƭ
	NALU_TYPE_DPB = 3, // �� IDR ֡�� B �����ݻ���Ƭ
	NALU_TYPE_DPC = 4, // �� IDR ֡�� C �����ݻ���Ƭ
	NALU_TYPE_IDR = 5, // IDR��Instantaneous Decoding Refresh����ʱ����ˢ�£� ֡������һ�� GOP �Ŀ�ͷ������������ˢ�£�ʹ�����´���
	NALU_TYPE_SEI = 6, // Supplemental enhancement information��������ǿ��Ϣ����������Ƶ���涨ʱ����Ϣ��һ�����������ͼ������֮ǰ����ĳЩӦ���У������Ա�ʡ��
	NALU_TYPE_SPS = 7, // Sequence Parameter Sets�����в�������������?�������Ƶ���е�ȫ�ֲ���
	NALU_TYPE_PPS = 8, // Picture Parameter Sets��ͼ�����������Ӧ����?��������ĳ?��ͼ�����ĳ?��ͼ��Ĳ���
	NALU_TYPE_AUD = 9, // �ֽ��
	NALU_TYPE_EOSEQ = 10, // ���н���
	NALU_TYPE_EOSTREAM = 11, // ��������
	NALU_TYPE_FILL = 12, // ���
} NaluType;

// NALU ���ȼ�����Ӧ NAL Header �� nal_ref_idc��ռ 2 bit��ȡֵԽ?����ʾ��ǰ NAL Խ��Ҫ����Ҫ�����ܵ�����
typedef enum {
	NALU_PRIORITY_DISPOSABLE = 0,
	NALU_PRIORITY_LOW = 1,
	NALU_PRIORITY_HIGH = 2,
	NALU_PRIORITY_HIGHEST = 3
} NaluPriority;

// NALU ��Ԫ
typedef struct
{
	// NAL Header��1 Byte
	int forbidden_bit; // ��ֹλ��1 bit
	int nal_reference_idc; // ���ȼ���2 bit
	int nal_unit_type; // ���ͣ�5 bit
	// ԭʼ�ֽ����и��ɣ�RBSP��Raw Byte Sequence Payload��
	int startcodeprefix_len; // StartCode �ĳ��ȣ�4 for parameter sets and first slice in picture, 3 for everything else (suggested)
	unsigned len; // Length of the NAL unit (Excluding the start code, which does not belong to the NALU)
	unsigned max_size; // Nal Unit Buffer size

	char *buf; // contains the first byte followed by the EBSP
} NALU_t;

FILE *h264bitstream = NULL; // the bit stream file

int info2 = 0, info3 = 0;

// �ҵ� 3 �ֽڵ� StartCode
static int FindStartCode2(unsigned char *Buf)
{
	if (Buf[0] != 0 || Buf[1] != 0 || Buf[2] != 1)
		return 0; // 0x000001
	else
		return 1;
}

// �ҵ� 4 �ֽڵ� StartCode
static int FindStartCode3(unsigned char *Buf)
{
	if (Buf[0] != 0 || Buf[1] != 0 || Buf[2] != 0 || Buf[3] != 1)
		return 0; // 0x00000001
	else
		return 1;
}

int GetAnnexbNALU(NALU_t *nalu)
{
	int pos = 0;
	int StartCodeFound, rewind;
	unsigned char *Buf;

	// �� Buf ���� nalu->max_size �Ŀռ�
	if ((Buf = (unsigned char*)calloc(nalu->max_size, sizeof(char))) == NULL)
		printf("GetAnnexbNALU: Could not allocate Buf memory.\n");

	nalu->startcodeprefix_len = 3;

	// ������� H.264 ���ļ�û�� 3 bits������
	if (3 != fread(Buf, 1, 3, h264bitstream))
	{
		free(Buf);
		return 0;
	}
	info2 = FindStartCode2(Buf);
	if (info2 != 1) // StartCode �ĳ��Ȳ��� 3 �ֽ�
	{
		if (1 != fread(Buf + 3, 1, 1, h264bitstream))
		{
			free(Buf);
			return 0;
		}
		info3 = FindStartCode3(Buf);
		if (info3 != 1) // StartCode �ĳ��Ȳ��� 3 ���� 4 �ֽڣ�����
		{
			free(Buf);
			return -1;
		}
		else // StartCode �ĳ���Ϊ 4 �ֽ�
		{
			pos = 4;
			nalu->startcodeprefix_len = 4;
		}
	}
	else // StartCode �ĳ���Ϊ 3 �ֽ�
	{
		nalu->startcodeprefix_len = 3;
		pos = 3;
	}

	StartCodeFound = 0;
	info2 = 0;
	info3 = 0;

	while (!StartCodeFound)
	{
		if (feof(h264bitstream))
		{
			nalu->len = (pos - 1) - nalu->startcodeprefix_len;
			memcpy(nalu->buf, &Buf[nalu->startcodeprefix_len], nalu->len);
			nalu->forbidden_bit = nalu->buf[0] & 0x80; // 1 bit
			nalu->nal_reference_idc = nalu->buf[0] & 0x60; // 2 bit
			nalu->nal_unit_type = (nalu->buf[0]) & 0x1f; // 5 bit
			free(Buf);
			return pos - 1;
		}
		Buf[pos++] = fgetc(h264bitstream);
		// �� Buf ����� 4 �ֽڣ�������û�� StartCode
		info3 = FindStartCode3(&Buf[pos - 4]);
		if (info3 != 1)
		{
			// �ٶ� Buf ����� 3 �ֽڣ�������û�� StartCode
			info2 = FindStartCode2(&Buf[pos - 3]);
		}
		StartCodeFound = (info2 == 1 || info3 == 1);
	}

	// Here, we have found another start code, and read length of startcode bytes more than we should have.
	// Hence, go back in the file.
	rewind = (info3 == 1) ? -4 : -3; // ���˵��ֽ���

	if (0 != fseek(h264bitstream, rewind, SEEK_CUR))
	{
		free(Buf);
		printf("GetAnnexbNALU: Can not fseek in the bit stream file.\n");
	}

	// Here the Start code, the complete NALU, and the next start code is in the Buf.  
	// The size of Buf is pos, pos+rewind are the number of bytes excluding the next start code,
	// and (pos+rewind)-startcodeprefix_len is the size of the NALU excluding the start code.

	nalu->len = (pos + rewind) - nalu->startcodeprefix_len;
	// nalu->buf ֻ�洢 NALU ��Ԫ������ StartCode
	memcpy(nalu->buf, &Buf[nalu->startcodeprefix_len], nalu->len);
	// NAL Header
	nalu->forbidden_bit = nalu->buf[0] & 0x80; // 1 bit
	nalu->nal_reference_idc = nalu->buf[0] & 0x60; // 2 bit
	nalu->nal_unit_type = (nalu->buf[0]) & 0x1f; // 5 bit

	free(Buf);

	return (pos + rewind); // ���� StartCode + һ�� NALU �ĳ���
}

/**
* Analysis H.264 Bitstream
* @param url Location of input H.264 bitstream file.
*/
int simplest_h264_parser(const char *url)
{

	NALU_t *n;
	int buffersize = 100000;

	FILE *myout = stdout;
	// FILE *myout = fopen("output_log.txt", "wb+");

	h264bitstream = fopen(url, "rb+");
	if (h264bitstream == NULL)
	{
		printf("Open file error.\n");
		return 0;
	}

	n = (NALU_t*)calloc(1, sizeof(NALU_t));
	if (n == NULL)
	{
		printf("Alloc NALU error.\n");
		return 0;
	}

	n->max_size = buffersize;
	n->buf = (char*)calloc(buffersize, sizeof(char));
	if (n->buf == NULL)
	{
		free(n);
		printf("Alloc NALU: n->buf error.\n");
		return 0;
	}

	int data_offset = 0;
	int nalu_cnt = 0;
	printf("-----+---------+------ NALU Table ---+--------+---------+\n");
	printf(" NUM |   POS   | FORBIDDEN |   IDC   |  TYPE  |   LEN   |\n");
	printf("-----+---------+-----------+---------+--------+---------+\n");

	while (!feof(h264bitstream))
	{
		int nalu_length = GetAnnexbNALU(n);

		char type_str[20] = { 0 };
		switch (n->nal_unit_type)
		{
		case NALU_TYPE_SLICE: sprintf(type_str, "SLICE"); break;
		case NALU_TYPE_DPA: sprintf(type_str, "DPA"); break;
		case NALU_TYPE_DPB: sprintf(type_str, "DPB"); break;
		case NALU_TYPE_DPC: sprintf(type_str, "DPC"); break;
		case NALU_TYPE_IDR: sprintf(type_str, "IDR"); break;
		case NALU_TYPE_SEI: sprintf(type_str, "SEI"); break;
		case NALU_TYPE_SPS: sprintf(type_str, "SPS"); break;
		case NALU_TYPE_PPS: sprintf(type_str, "PPS"); break;
		case NALU_TYPE_AUD: sprintf(type_str, "AUD"); break;
		case NALU_TYPE_EOSEQ: sprintf(type_str, "EOSEQ"); break;
		case NALU_TYPE_EOSTREAM: sprintf(type_str, "EOSTREAM"); break;
		case NALU_TYPE_FILL: sprintf(type_str, "FILL"); break;
		default: sprintf(type_str, "unknown"); break;
		}
		char idc_str[20] = { 0 };
		switch (n->nal_reference_idc >> 5)
		{
		case NALU_PRIORITY_DISPOSABLE: sprintf(idc_str, "DISPOS"); break;
		case NALU_PRIORITY_LOW: sprintf(idc_str, "LOW"); break;
		case NALU_PRIORITY_HIGH: sprintf(idc_str, "HIGH"); break;
		case NALU_PRIORITY_HIGHEST: sprintf(idc_str, "HIGHEST"); break;
		default: sprintf(type_str, "unknown"); break;
		}

		fprintf(myout, "%5d|%9d|%11d|%9s|%8s|%9d|\n", nalu_cnt, data_offset, n->forbidden_bit, idc_str, type_str, n->len);

		data_offset = data_offset + nalu_length;

		nalu_cnt++;
	}

	// Free
	if (n)
	{
		if (n->buf)
		{
			free(n->buf);
			n->buf = NULL;
		}
		free(n);
	}
	return 0;
}

int main()
{
	simplest_h264_parser("sintel.h264");

	system("pause");
	return 0;
}