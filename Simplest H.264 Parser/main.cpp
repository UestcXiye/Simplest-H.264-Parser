/**
* 最简单的 H.264 视频码流解析程序
* Simplest H.264 Parser
*
* 原程序：
* 雷霄骅 Lei Xiaohua
* leixiaohua1020@126.com
* 中国传媒大学/数字电视技术
* Communication University of China / Digital TV Technology
* http://blog.csdn.net/leixiaohua1020
*
* 修改：
* 刘文晨 Liu Wenchen
* 812288728@qq.com
* 电子科技大学/电子信息
* University of Electronic Science and Technology of China / Electronic and Information Science
* https://blog.csdn.net/ProgramNovice
*
* 本项目是一个 H.264 码流分析程序，可以分离并解析 NALU。
*
* This project is a H.264 stream analysis program.
* It can parse H.264 bitstream and analysis NALU of stream.
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 解决报错：fopen() 函数不安全
#pragma warning(disable:4996)

// NALU 类型取值，对应 NAL Header 的 nal_unit_type，占 5 bit
typedef enum {
	NALU_TYPE_SLICE = 1, // 非 IDR 帧中不采用数据划分的片
	NALU_TYPE_DPA = 2, // 非 IDR 帧中 A 类数据划分片
	NALU_TYPE_DPB = 3, // 非 IDR 帧中 B 类数据划分片
	NALU_TYPE_DPC = 4, // 非 IDR 帧中 C 类数据划分片
	NALU_TYPE_IDR = 5, // IDR（Instantaneous Decoding Refresh，即时解码刷新） 帧，它是一个 GOP 的开头，作用是立刻刷新，使错误不致传播
	NALU_TYPE_SEI = 6, // Supplemental enhancement information：附加增强信息，包含了视频画面定时等信息，一般放在主编码图像数据之前，在某些应用中，它可以被省略
	NALU_TYPE_SPS = 7, // Sequence Parameter Sets，序列参数集，保存了?组编码视频序列的全局参数
	NALU_TYPE_PPS = 8, // Picture Parameter Sets，图像参数集，对应的是?个序列中某?幅图像或者某?幅图像的参数
	NALU_TYPE_AUD = 9, // 分界符
	NALU_TYPE_EOSEQ = 10, // 序列结束
	NALU_TYPE_EOSTREAM = 11, // 码流结束
	NALU_TYPE_FILL = 12, // 填充
} NaluType;

// NALU 优先级，对应 NAL Header 的 nal_ref_idc，占 2 bit。取值越?，表示当前 NAL 越重要，需要优先受到保护
typedef enum {
	NALU_PRIORITY_DISPOSABLE = 0,
	NALU_PRIORITY_LOW = 1,
	NALU_PRIORITY_HIGH = 2,
	NALU_PRIORITY_HIGHEST = 3
} NaluPriority;

// NALU 单元
typedef struct
{
	// NAL Header，1 Byte
	int forbidden_bit; // 禁止位，1 bit
	int nal_reference_idc; // 优先级，2 bit
	int nal_unit_type; // 类型，5 bit
	// 原始字节序列负荷（RBSP，Raw Byte Sequence Payload）
	int startcodeprefix_len; // StartCode 的长度，4 for parameter sets and first slice in picture, 3 for everything else (suggested)
	unsigned len; // Length of the NAL unit (Excluding the start code, which does not belong to the NALU)
	unsigned max_size; // Nal Unit Buffer size

	char *buf; // contains the first byte followed by the EBSP
} NALU_t;

FILE *h264bitstream = NULL; // the bit stream file

int info2 = 0, info3 = 0;

// 找到 3 字节的 StartCode
static int FindStartCode2(unsigned char *Buf)
{
	if (Buf[0] != 0 || Buf[1] != 0 || Buf[2] != 1)
		return 0; // 0x000001
	else
		return 1;
}

// 找到 4 字节的 StartCode
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

	// 给 Buf 分配 nalu->max_size 的空间
	if ((Buf = (unsigned char*)calloc(nalu->max_size, sizeof(char))) == NULL)
		printf("GetAnnexbNALU: Could not allocate Buf memory.\n");

	nalu->startcodeprefix_len = 3;

	// 若输入的 H.264 流文件没有 3 bits，返回
	if (3 != fread(Buf, 1, 3, h264bitstream))
	{
		free(Buf);
		return 0;
	}
	info2 = FindStartCode2(Buf);
	if (info2 != 1) // StartCode 的长度不是 3 字节
	{
		if (1 != fread(Buf + 3, 1, 1, h264bitstream))
		{
			free(Buf);
			return 0;
		}
		info3 = FindStartCode3(Buf);
		if (info3 != 1) // StartCode 的长度不是 3 或者 4 字节，错误
		{
			free(Buf);
			return -1;
		}
		else // StartCode 的长度为 4 字节
		{
			pos = 4;
			nalu->startcodeprefix_len = 4;
		}
	}
	else // StartCode 的长度为 3 字节
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
		// 读 Buf 的最后 4 字节，看看有没有 StartCode
		info3 = FindStartCode3(&Buf[pos - 4]);
		if (info3 != 1)
		{
			// 再读 Buf 的最后 3 字节，看看有没有 StartCode
			info2 = FindStartCode2(&Buf[pos - 3]);
		}
		StartCodeFound = (info2 == 1 || info3 == 1);
	}

	// Here, we have found another start code, and read length of startcode bytes more than we should have.
	// Hence, go back in the file.
	rewind = (info3 == 1) ? -4 : -3; // 回退的字节数

	if (0 != fseek(h264bitstream, rewind, SEEK_CUR))
	{
		free(Buf);
		printf("GetAnnexbNALU: Can not fseek in the bit stream file.\n");
	}

	// Here the Start code, the complete NALU, and the next start code is in the Buf.  
	// The size of Buf is pos, pos+rewind are the number of bytes excluding the next start code,
	// and (pos+rewind)-startcodeprefix_len is the size of the NALU excluding the start code.

	nalu->len = (pos + rewind) - nalu->startcodeprefix_len;
	// nalu->buf 只存储 NALU 单元，不含 StartCode
	memcpy(nalu->buf, &Buf[nalu->startcodeprefix_len], nalu->len);
	// NAL Header
	nalu->forbidden_bit = nalu->buf[0] & 0x80; // 1 bit
	nalu->nal_reference_idc = nalu->buf[0] & 0x60; // 2 bit
	nalu->nal_unit_type = (nalu->buf[0]) & 0x1f; // 5 bit

	free(Buf);

	return (pos + rewind); // 返回 StartCode + 一个 NALU 的长度
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