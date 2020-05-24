#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <time.h>
#include <stdint.h>
#include <math.h>
#include "utility.h"
#include "nmea_protocol.h"
#include "service_log.h"
#include "kk_os_header.h"
#define MAX_TYPE_LEN 12

// AGPS信息（位置,时间,频率）
typedef struct  
{
	double lat;
	double lng;
	double alt;
	double tow;
	float df;
	float pos_acc;
	float t_acc; 
	float f_acc; 
	U32	res;
	U16 wn;
	U8 	time_source;
	U8	flags;// BIT0:位置有效标志 BIT1:时间有效标志
} ATAidInfo;


static S32 hex2int(char c);

static U8 nmea_checksum(const char* p_sentence);

static bool nmea_check(const char* p_sentence, bool strict);

static bool nmea_isfield(char c);

static bool nmea_scan(const char* p_sentence, const char *format, ...);

//static bool nmea_create_td_sentence(const U16 cmd,const U8* p_data,const U16 data_len, U8* p_sentence, U8* p_len);


static S32 hex2int(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F')
	{
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    return -1;
}



/**
 * Function:   检查语句有效性和校验和
 * Description:不检查语句完整性
 * Input:	   无
 * Output:	   无
 * Return:	   true——有效；false——无效
 * Others:	   
 */
static bool nmea_check(const char* p_sentence, bool strict)
{
    U8 checksum = 0;
	S32 upper = 0;
	S32 lower = 0;
	S32 expected = 0;
	const char* org_sentence = p_sentence;

    // 语句长度限制
    if (strlen(p_sentence) > SENTENCE_MAX_LENGTH + 3)
    {
    	
        return false;
    }

    // 有效的语句应该以'$'开头
    if (*p_sentence++ != '$')
    {
    	LOG(LEVEL_WARN,"It is not begin with $!");
        return false;
    }

    while (*p_sentence && *p_sentence != '*' && util_isprint((U8)*p_sentence))
    {
        checksum ^= *p_sentence++;
    }

	// 有校验和
    if (*p_sentence == '*') 
	{
        p_sentence++;
		
        upper = hex2int(*p_sentence++);
        if (upper == -1)
        {
        	LOG(LEVEL_WARN,"upper is -1");
            return false;
        }
		
        lower = hex2int(*p_sentence++);
        if (lower == -1)
        {
        	LOG(LEVEL_WARN,"lower is -1");
            return false;
        }
		
        expected = upper << 4 | lower;
        if (checksum != expected)
        {
        	LOG(LEVEL_WARN, "checksum=%d,expected=%d", checksum,expected);
            return false;
        }
    } 
	else if (strict) 
	{
        // 严格模式下没有校验和的数据是无效的
        LOG(LEVEL_WARN,"It has not checksum:%s",org_sentence);
        return false;
    }

    // 最后一定要是换行
    if (strict && *p_sentence && strcmp(p_sentence, "\n") && strcmp(p_sentence, "\r\n"))
    {
    	LOG(LEVEL_WARN," It has not end flag:%s",org_sentence);
        return false;
    }

    return true;
}

static bool nmea_isfield(char c) 
{
    return (bool)(util_isprint((const char)c) && c != ',' && c != '*');
}

/**
 * Function:   从语句中提取变量
 * Description:
 * Input:	   p_sentence:语句；format——格式；
 * Output:	   ...——变量
 * Return:	   true——成功；false——失败
 * Others:	   类似于scanf的处理方式,支持下面几种格式:
               c - single character (char*)
               d - direction, returned as 1/-1, default 0 (S32*)
               f - fractional, returned as value + scale (S32*, S32*)
               i - decimal, default zero (S32*)
               s - string (char*)
               t - talker identifier and type (char*)
               T - date/time stamp (S32*, S32*, S32*)              
 */         
static bool nmea_scan(const char* p_sentence, const char* p_format, ...)

{
    bool result = false;
    bool optional = false;
	const char* p_field = p_sentence;
	char type = 0;
	char value_8 = 0;
	S32 value_32 = 0;
	S32 sign_32 = 0;
    S32 scale_32 = 0;
	S32 digit_32 = 0;
	char* p_buf = NULL;
	U8 index = 0;
	S32 d = 0;
	S32 m = 0;
	S32 y = 0;
	char dArr[3] = {0};
	char mArr[3] = {0};
	char yArr[3] = {0};
	char hArr[3] = {0};
	char iArr[3] = {0};
	char sArr[3] = {0};
	NMEAFloat fraction = {0};
	NMEATime* p_time = NULL;
	S32 h = -1;
	S32 i = -1;
	S32 s = -1;
	S32 u = -1;

	
    va_list ap;
    va_start(ap, p_format);

    while (*p_format) 
	{
        type = *p_format++;

        if (type == ';') 
		{
            // 后面所有的域都是可选的
            optional = true;
            continue;
        }

        if (!p_field && !optional) 
		{
            goto parse_error;
        }

        switch (type) 
		{
			// Single character field(char)
            case 'c': 
			{
                value_8 = '\0';
                if (p_field && nmea_isfield(*p_field))
                {
                    value_8 = *p_field;
                }

                *va_arg(ap, char*) = value_8;
            } 
			break;
			// Single character direction field(S32)
            case 'd': 
			{
                value_32 = 0;

                if (p_field && nmea_isfield(*p_field)) 
				{
                    switch (*p_field) 
					{
                        case 'N':
                        case 'E':
                            value_32 = 1;
                            break;
                        case 'S':
                        case 'W':
                            value_32 = -1;
                            break;
                        default:
                            goto parse_error;
                    }
                }

                *va_arg(ap, S32*) = value_32;
            } 
			break;

			// Fractional value with scale (NMEAFloat).
            case 'f': 
			{
                sign_32 = 0;
                value_32 = -1;
                scale_32 = 0;

                if (p_field) 
				{
                    while (nmea_isfield(*p_field)) 
					{
                        if (*p_field == '+' && !sign_32 && value_32 == -1) 
						{
                            sign_32 = 1;
                        } 
						else if (*p_field == '-' && !sign_32 && value_32 == -1) 
						{
                            sign_32 = -1;
                        } 
						else if (util_isdigit((U8) *p_field)) 
						{
                            digit_32 = *p_field - '0';
                            if (value_32 == -1)
                            {
                                value_32 = 0;
                            }
                            if (value_32 > (INT_LEAST32_MAX - digit_32) / 10) 
							{
                                /* we ran out of bits, what do we do? */
                                if (scale_32) 
								{
                                    /* truncate extra precision */
                                    break;
                                }
								else 
								{
                                    /* integer overflow. bail out. */
                                    goto parse_error;
                                }
                            }
                            value_32 = (10 * value_32) + digit_32;
                            scale_32 *= 10;
                           
                        }
						else if (*p_field == '.' && scale_32 == 0) 
						{
                            scale_32 = 1;
                        } 
						else if (*p_field == ' ') 
						{
                            /* Allow spaces at the start of the p_field. Not NMEA
                             * conformant, but some modules do this. */
                            if (sign_32 != 0 || value_32 != -1 || scale_32 != 0)
                            {
                                goto parse_error;
                            }
                        } 
						else 
						{
                            goto parse_error;
                        }
                        p_field++;
                    }
                }

                if ((sign_32 || scale_32) && value_32 == -1)
                {
                    goto parse_error;
                }

                if (value_32 == -1) 
				{
                    /* No digits were scanned. */
                    value_32 = 0;
                    scale_32 = 0;
                } 
				else if (scale_32 == 0) 
				{
                    /* No decimal point. */
                    scale_32 = 1;
                }
                if (sign_32)
                {
                    value_32 *= sign_32;
                }
				fraction.value = value_32;
				fraction.scale = scale_32;
                *va_arg(ap, NMEAFloat*) = fraction;
            } 
			break;
			
			// Integer value, default 0 (S32).
            case 'i': 
			{ 
                value_32 = 0;

                if (p_field) 
				{	
                    char *endptr = NULL;
                    value_32 = util_strtol(p_field, &endptr);
                    if (nmea_isfield(*endptr))
                    {
                        goto parse_error;
                    }
                }
                *va_arg(ap, S32*) = value_32;
            } 
			break;

			// String value (char *).
            case 's': 
			{ 
                p_buf = va_arg(ap, char *);

                if (p_field) 
				{
                    while (nmea_isfield(*p_field))
                    {
                        *p_buf++ = *p_field++;
                    }
                }

                *p_buf = '\0';
            } 
			break;

			// NMEA talker+sentence identifier (char*).
            case 't': 
			{ 
                // This p_field is always mandatory.
                if (!p_field)
                {
                    goto parse_error;
                }

                if (p_field[0] != '$')
                {
                    goto parse_error;
                }
				
                while(nmea_isfield(p_field[index]) && index < MAX_TYPE_LEN)
                {
             		index++;
                }
				index--;

                p_buf = va_arg(ap, char*);
                memcpy(p_buf, p_field+1, index);
                p_buf[index] = '\0';
            }
			break;

			// Date (S32, S32, S32), -1 if empty.
            case 'D': 
			{ 
                NMEADate *date = va_arg(ap, NMEADate *);

                d = -1;
				m = -1;
				y = -1;

                if (p_field && nmea_isfield(*p_field)) 
				{
                    // Always six digits.
                    for (index = 0; index < 6; index++)
                    {
                        if (!util_isdigit((U8) p_field[index]))
                        {
                            goto parse_error;
                        }
                    }

                    dArr[0] = p_field[0];
					dArr[1] = p_field[1];
					dArr[2] = '\0';
					mArr[0] = p_field[2];
					mArr[1] = p_field[3];
					mArr[2] = '\0';
					yArr[0] = p_field[4];
					yArr[1] = p_field[5];
					yArr[2] = '\0';
                    d = util_strtol(dArr, NULL);
                    m = util_strtol(mArr, NULL);
                    y = util_strtol(yArr, NULL);
                }

                date->day = d;
                date->month = m;
                date->year = y;
            } 
			break;
			
			// Time (S32, S32, S32, S32), -1 if empty.
            case 'T': 
			{
                p_time = va_arg(ap, NMEATime *);

                h = -1;
				i = -1;
				s = -1;
				u = -1;

                if (p_field && nmea_isfield(*p_field))
				{
                    // Minimum required: integer time.
                    for (index = 0; index < 6; index++)
                    {
                        if (!util_isdigit((U8) p_field[index]))
                        {
                            goto parse_error;
                        }
                    }

					hArr[0] = p_field[0];
					hArr[1] = p_field[1];
					hArr[2] = '\0';
					iArr[0] = p_field[2];
					iArr[1] = p_field[3];
					iArr[2] = '\0';
					sArr[0] = p_field[4];
					sArr[1] = p_field[5];
					sArr[2] = '\0';

                    h = util_strtol(hArr, NULL);
                    i = util_strtol(iArr, NULL);
                    s = util_strtol(sArr, NULL);
                    p_field += 6;

                    // Extra: fractional time. Saved as microseconds.
                    if (*p_field++ == '.') 
					{
                        value_32 = 0;
                        scale_32 = 1000000LU;
                        while (util_isdigit((U8) *p_field) && scale_32 > 1) 
						{
                            value_32 = (value_32 * 10) + (*p_field++ - '0');
                            scale_32 /= 10;
                        }
                        u = value_32 * scale_32;
                    }
					else 
					{
                        u = 0;
                    }
                }

                p_time->hours = h;
                p_time->minutes = i;
                p_time->seconds = s;
                p_time->microseconds = u;
            } break;

			// Ignore the p_field.
            case '_':
			{ 
            } 
			break;

            default:
			{ 
                goto parse_error;
            }
        }

        /* Progress to the next p_field. */
    	while (nmea_isfield(*p_sentence))
    	{
        	p_sentence++;
    	}
    	/* Make sure there is a p_field there. */ 
    	if (*p_sentence == ',') 
		{ 
        	p_sentence++;
        	p_field = p_sentence;
    	} 
		else 
		{
        	p_field = NULL;
    	}
    }

    result = true;

parse_error:
    va_end(ap);
    return result;
}

NMEASentenceID nmea_sentence_id(const char* p_sentence, const U16 len, bool strict)
{
	char type[MAX_TYPE_LEN] = {0};
	U8 head_buff[3] = {0};
	//U16 head = 0;
	//U16 cmd = 0;

	memcpy(head_buff,p_sentence,6);
	//head = head_buff[0];

    if(MKWORD(head_buff[0],head_buff[1]) == HD_HEADER)
    {
        return HD_SENTENCE_ID;
    }

    if (!nmea_scan(p_sentence, "t", type))
    {
        //LOG(ERR,"sentence %s",p_sentence);
        //LOG(ERR,"p_sentence error!! invalid");
        return NMEA_INVALID;
    }

	if (!strcmp(type, "PMTK705"))
    {
        return NMEA_SENTENCE_MTK_VER;
    }
    else if (!strcmp(type+2, "RMC"))
    {
        return NMEA_SENTENCE_RMC;
    }
    else if (!strcmp(type+2, "GGA"))
    {
        return NMEA_SENTENCE_GGA;
    }
    else if (!strcmp(type+2, "GSA"))
    {
        return NMEA_SENTENCE_GSA;
    }
    else if (!strcmp(type+2, "GLL"))
    {
        return NMEA_SENTENCE_GLL;
    }
    else if (!strcmp(type+2, "GST"))
    {
        return NMEA_SENTENCE_GST;
    }
    else if (!strcmp(type, "GPGSV"))
    {
        return NMEA_SENTENCE_GSV;
    }
    else if (!strcmp(type, "BDGSV"))
    {
        return NMEA_SENTENCE_BDGSV;
    }
    else if (!strcmp(type+2, "VTG"))
    {
        return NMEA_SENTENCE_VTG;
    }
    else if (!strcmp(type+2, "ZDA"))
    {
        return NMEA_SENTENCE_ZDA;
    }
    else if (!strcmp(type+2, "TXT"))
    {
        return NMEA_SENTENCE_TXT;
    }
	else if (!strcmp(type+2, "ACCURACY"))
    {
        return NMEA_SENTENCE_ACCURACY;
    }
	else
	{
		//LOG(DEBUG,"Unknown type:%s",type);
	    return NMEA_UNKNOWN;
	}
}

static U8 nmea_checksum(const char* p_sentence)
{
	U8 checksum = 0;
	//跳过'$'
    if (*p_sentence == '$')
    {
        p_sentence++;
    }

    // 校验和算法是计算'$'和'*'之间的所有字节的异或
    while (*p_sentence && *p_sentence != '*')
    {
        checksum ^= *p_sentence++;
    }
    return checksum;
}


//$GPTXT,01,01,02,MA=CASIC*27
//$GPTXT,01,01,02,IC=AT6558-5N-32-1C510800*48
//$$GPTXT,01,01,02,SW=URANUS5,V5.1.0.0*1F
//$$GPTXT,01,01,02,TB=2018-04-18,10:28:16*40
//$GPTXT,01,01,02,MO=GB*77
//$GNTXT,02,01,02,ALLYSTAR,8221*79
//$GNTXT,02,02,02,ANT_OPEN*43
bool nmea_parse_txt(NMEASentenceVER* p_frame, const char* p_sentence)
{
	char type[MAX_TYPE_LEN] = {0};
	//不知道前面这三个数字是什么意思
	U8 a = 0;
	U8 b = 0;
	U8 c = 0;
	char str1[64] = {0};
	char str2[64] = {0};
    if (!nmea_scan(p_sentence, "tiiis;s",
            type,
            &a,
            &b,
            &c, 
            str1,
            str2))
    {
    	LOG(LEVEL_DEBUG,"Failed to parse TXT:%s",p_sentence);
        return false;
    }
   
    if (strcmp(type+2, "TXT"))
    {
    	LOG(LEVEL_DEBUG,"Failed to parse type:%s",type+2);
        return false;
    }
	
	if(a == 2 && b == 2)
	{
        //memcpy(p_frame->ver,str1,strlen(str1));
        //memcpy(p_frame->release_str,str2,strlen(str2));

        return true;
    }
    else if(a == 1 && b == 1)
    {
        memcpy(p_frame->ant_state,str1,strlen(str1));
    }

    
    return false;
}

//$TDINF,Techtotop Multi-GNSS Receiver*63
//$TDINF,T3,RomFw,1.1(48),Aug 12 2016 16:57:35*02
//$TDINF,GNSS=BDS+GPS,ANT=Disable*6D
//ic:T3;sw:RomFw;version:1.1(48)
bool nmea_parse_inf(NMEASentenceVER* p_frame, const char* p_sentence)
{
	char type[MAX_TYPE_LEN] = {0};
	char str1[32] = {0};
	char str2[32] = {0};
	char str3[32] = {0};
	char str4[32] = {0};
	if (!nmea_scan(p_sentence, "ts;sss",
			type,
			str1,
			str2,
			str3,
			str4))
	{
		return false;
	}
	if (strcmp(type, "TDINF"))
	{
		LOG(LEVEL_WARN, "type(%s) is not TDINF", type);
		return false;
	}

	if (!strcmp(str1, "T3"))
	{
		memcpy(p_frame->ver, str1, strlen(str1));
		if (strlen(str2) != 0)
		{
			memcpy(p_frame->ver + strlen(p_frame->ver), ",", 1);
			memcpy(p_frame->ver + strlen(p_frame->ver), str2, strlen(str2));
		}
		if (strlen(str3) != 0)
		{
			memcpy(p_frame->ver + strlen(p_frame->ver), ",", 1);
			memcpy(p_frame->ver + strlen(p_frame->ver), str3, strlen(str3));
		}
		if (strlen(str4) != 0)
		{
			memcpy(p_frame->ver + strlen(p_frame->ver), ",", 1);
			memcpy(p_frame->ver + strlen(p_frame->ver), str4, strlen(str4));
		}
	}
	else
	{
		return false;
	}
	return true;
}

//泰斗:  $GNRMC,032713.00,A,2232.53348,N,11357.15840,E,0.624,,250619,,,A,V*18
//中科微:$GNRMC,080452.042,V,,,,,,,190319,,,M,V*24
bool nmea_parse_rmc(NMEASentenceRMC* p_frame, const char* p_sentence)
{
    char type[MAX_TYPE_LEN] = {0};
    char validity = 0;
    S32 latitude_direction = 0;
    S32 longitude_direction = 0;
    S32 variation_direction = 0;
    if (!nmea_scan(p_sentence, "t;TcfdfdffDfd",
            type,
            &p_frame->time,
            &validity,
            &p_frame->latitude, 
            &latitude_direction,
            &p_frame->longitude,
            &longitude_direction,
            &p_frame->speed,
            &p_frame->course,
            &p_frame->date,
            &p_frame->variation,
            &variation_direction))
    {
        return false;
    }
    if (strcmp(type+2, "RMC"))
    {
        return false;
    }

    p_frame->valid = (bool)(validity == 'A');
    p_frame->latitude.value *= latitude_direction;
    p_frame->longitude.value *= longitude_direction;
    p_frame->variation.value *= variation_direction;

    return true;
}


//泰斗:     $GNGGA,081243.00,,,,,0,00,99.9,,,,,,*4D
//中科微:$GNGGA,080458.042,,,,,0,00,25.5,,,,,,*7D
bool nmea_parse_gga(NMEASentenceGGA* p_frame, const char* p_sentence)
{
    char type[MAX_TYPE_LEN] = {0};
    S32 latitude_direction = 0;
    S32 longitude_direction = 0;

    if (!nmea_scan(p_sentence, "tTfdfdiiffcfcf_",
            type,
            &p_frame->time,
            &p_frame->latitude, &latitude_direction,
            &p_frame->longitude, &longitude_direction,
            &p_frame->fix_quality,
            &p_frame->satellites_tracked,
            &p_frame->hdop,
            &p_frame->altitude, &p_frame->altitude_units,
            &p_frame->height, &p_frame->height_units,
            &p_frame->dgps_age))
    {
        return false;
    }
    if (strcmp(type+2, "GGA"))
    {
        return false;
    }

    p_frame->latitude.value *= latitude_direction;
    p_frame->longitude.value *= longitude_direction;

    return true;
}

//泰斗:     $GNGSA,A,1,,,,,,,,,,,,,99.9,99.9,99.9,1*0A
//中科微: $GNGSA,A,1,,,,,,,,,,,,,25.5,25.5,25.5,4*04
//MTK:    $GPGSA,A,3,30,28,19,,,,,,,,,,2.28,1.39,1.81*08

bool nmea_parse_gsa(NMEASentenceGSA* p_frame, const char* p_sentence)
{
    char type[MAX_TYPE_LEN] = {0};

	p_frame->fix_type = NMEA_GPGSA_FIX_3D;

    if (!nmea_scan(p_sentence, "tci;iiiiiiiiiiiifff",
            type,
            &p_frame->mode,
            &p_frame->fix_type,
            &p_frame->satellites[0],
            &p_frame->satellites[1],
            &p_frame->satellites[2],
            &p_frame->satellites[3],
            &p_frame->satellites[4],
            &p_frame->satellites[5],
            &p_frame->satellites[6],
            &p_frame->satellites[7],
            &p_frame->satellites[8],
            &p_frame->satellites[9],
            &p_frame->satellites[10],
            &p_frame->satellites[11],
            &p_frame->pdop,
            &p_frame->hdop,
            &p_frame->vdop))
	{
		LOG(LEVEL_WARN, "Failed to parse GSA,sentence:%s", p_sentence);
		return false;
	}

    if (strcmp(type+2, "GSA"))
	{
		return false;
	}
    return true;
}

bool nmea_parse_gll(NMEASentenceGLL* p_frame, const char* p_sentence)
{
    char type[MAX_TYPE_LEN] = {0};
    S32 latitude_direction = 0;
    S32 longitude_direction = 0;

    if (!nmea_scan(p_sentence, "tfdfdTc;c",
            type,
            &p_frame->latitude, &latitude_direction,
            &p_frame->longitude, &longitude_direction,
            &p_frame->time,
            &p_frame->status,
            &p_frame->mode))
	{
		return false;
	}

    if (strcmp(type+2, "GLL"))
	{
		return false;
	}

    p_frame->latitude.value *= latitude_direction;
    p_frame->longitude.value *= longitude_direction;

    return true;
}

bool nmea_parse_gst(NMEASentenceGST*p_frame, const char* p_sentence)
{
    char type[MAX_TYPE_LEN] = {0};

    if (!nmea_scan(p_sentence, "tTfffffff",
            type,
            &p_frame->time,
            &p_frame->rms_deviation,
            &p_frame->semi_major_deviation,
            &p_frame->semi_minor_deviation,
            &p_frame->semi_major_orientation,
            &p_frame->latitude_error_deviation,
            &p_frame->longitude_error_deviation,
            &p_frame->altitude_error_deviation))
	{
		return false;
	}

    if (strcmp(type+2, "GST"))
	{
		return false;
	}

    return true;
}


//泰斗: 	  $GPGSV,4,2,14,09,34,250,,11,58,192,,16,24,064,,18,51,145,,0*66
//中科微:$GPGSV,3,1,10,01,29,175,,07,43,325,,08,66,357,,09,35,255,,0*6F
bool nmea_parse_gsv(NMEASentenceGSV* p_frame, const char* p_sentence)
{
    char type[MAX_TYPE_LEN] = {0};

    if (!nmea_scan(p_sentence, "tiii;iiiiiiiiiiiiiiii",
            type,
            &p_frame->total_msgs,
            &p_frame->msg_number,
            &p_frame->total_satellites,
            &p_frame->satellites[0].nr,
            &p_frame->satellites[0].elevation,
            &p_frame->satellites[0].azimuth,
            &p_frame->satellites[0].snr,
            &p_frame->satellites[1].nr,
            &p_frame->satellites[1].elevation,
            &p_frame->satellites[1].azimuth,
            &p_frame->satellites[1].snr,
            &p_frame->satellites[2].nr,
            &p_frame->satellites[2].elevation,
            &p_frame->satellites[2].azimuth,
            &p_frame->satellites[2].snr,
            &p_frame->satellites[3].nr,
            &p_frame->satellites[3].elevation,
            &p_frame->satellites[3].azimuth,
            &p_frame->satellites[3].snr
            ))  
	{
		return false;
	}
    
    if (strcmp(type+2, "GSV"))
    {
        return false;
    }
    return true;
}

bool nmea_parse_vtg(NMEASentenceVTG* p_frame, const char* p_sentence)
{
    char type[MAX_TYPE_LEN] = {0};
    char c_true = 0;
	char c_magnetic = 0;
	char c_knots = 0;
	char c_kph = 0;
	char c_faa_mode = 0;

    if (!nmea_scan(p_sentence, "t;fcfcfcfcc",
            type,
            &p_frame->true_track_degrees,
            &c_true,
            &p_frame->magnetic_track_degrees,
            &c_magnetic,
            &p_frame->speed_knots,
            &c_knots,
            &p_frame->speed_kph,
            &c_kph,
            &c_faa_mode))
	{
		return false;
	}

    if (strcmp(type+2, "VTG"))
	{
		return false;
	}

    // check chars
    if (c_true != 'T' || c_magnetic != 'M' || c_knots != 'N' || c_kph != 'K')
	{
		return false;
	}

    p_frame->faa_mode = (NMEAFAAMode)c_faa_mode;

    return true;
}

bool nmea_parse_zda(NMEASentenceZDA* p_frame, const char* p_sentence)
{
	char type[MAX_TYPE_LEN] = {0};

	if(!nmea_scan(p_sentence, "tTiiiii",
          type,
          &p_frame->time,
          &p_frame->date.day,
          &p_frame->date.month,
          &p_frame->date.year,
          &p_frame->hour_offset,
          &p_frame->minute_offset))
	{
		return false;
	}

	if (strcmp(type+2, "ZDA"))
	{
		return false;
	}

  	// check offsets
  	if (abs(p_frame->hour_offset) > 13 || p_frame->minute_offset > 59 || p_frame->minute_offset < 0)
	{
		return false;
	}
	return true;
}
time_t nmea_get_utc_time(const NMEADate* p_date, const NMEATime* p_time)
{
	struct tm utc_time = {0};

    if (NULL == p_date || NULL == p_time || p_date->year == -1 || p_time->hours == -1)
    {
        return 0;
    }
	
	// (-INFINITY,80)
    if (p_date->year < 80) 
	{
        utc_time.tm_year = 2000 + p_date->year - 1900; 
    }
	// [1900,+INFINITY)
	else if (p_date->year >= 1900) 
	{
        utc_time.tm_year = p_date->year - 1900;
    }
	// [80,1900)
	else 
	{
        utc_time.tm_year = p_date->year;
    }
    utc_time.tm_mon = p_date->month - 1;
    utc_time.tm_mday = p_date->day;
    utc_time.tm_hour = p_time->hours;
    utc_time.tm_min = p_time->minutes;
    utc_time.tm_sec = p_time->seconds;
    return util_mktime(&utc_time); 
}

S32 nmea_rescale(const NMEAFloat* p_fraction, S32 new_scale)
{
    if (p_fraction->scale == 0)
    {
        return 0;
    }
	
    if (p_fraction->scale == new_scale)
    {
        return p_fraction->value;
    }
	
    if (p_fraction->scale > new_scale)
    {
        return (p_fraction->value + ((p_fraction->value > 0) - (p_fraction->value < 0)) * p_fraction->scale/new_scale/2) / (p_fraction->scale/new_scale);
    }
    else
    {
        return p_fraction->value * (new_scale/p_fraction->scale);
    }
}


float nmea_tofloat(const NMEAFloat* p_fraction)
{
    if (p_fraction->scale == 0)
    {
        return 0;
    }
    return (float) p_fraction->value / (float) p_fraction->scale;
}

float nmea_tocoord(const NMEAFloat* p_fraction)
{
	S32 degrees = 0;
	S32 minutes = 0;
    if (p_fraction->scale == 0)
    {
        return 0;
    }
    degrees = p_fraction->value / (p_fraction->scale * 100);
    minutes = p_fraction->value % (p_fraction->scale * 100);
    return (float) degrees + (float) minutes / (60 * p_fraction->scale);
}

