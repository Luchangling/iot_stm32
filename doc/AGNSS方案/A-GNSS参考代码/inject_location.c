
static int
set_message(unsigned char* dst, unsigned char* src, int src_len)
{
    int i = 0;
    char checksum1 = 0;
    char checksum2 = 0;

    if (NULL == dst || NULL == src)
    {
        ALOGE("[%s:%d] : dst or src pointer is NULL ... Error", __FUNCTION__, __LINE__);
        return -1; /* No data to fill */
    }

    /* message header */
    for (i = 0; i < 4; i++)
    {
        dst[i] = *(src + i);
    }

    /* message len */
    dst[4] = src_len;
    dst[5] = 0;

    /* payload */
    for( i = 0 ; i < src_len ; i++)
    {
        dst[6 + i] = *(src + 4) ;
        src++;
    }

    /* check sum */
    for(i = 2; i < (6 + dst[4]); i++)
    {
        checksum1 += dst[i];
        checksum2 += checksum1;
    }

    dst[src_len + 6] = checksum1;
    dst[src_len + 7] = checksum2;

    return src_len + 8;
}

static int
hdbd_gps_inject_time(GpsUtcTime time, int64_t timeReference, int uncertainty)
{
    (void) (timeReference);
    (void) (uncertainty);
    int uncertainty_sec = 0;
    int uncertainty_nsec = 0;
    GpsState* s = _gps_state;

    unsigned char cmd[24] = {0}; /* packet head+payload */
    unsigned char message[30] = {0}; /* payload : 20, packet pad : 8 */
    int len = 0;
    time_t ntpRawTime;
    struct tm * utcNtp;

    memset(cmd, 0, sizeof(cmd));
    ntpRawTime = ((int64_t)time + getMonotonicMsCounter()-timeReference) / 1000;
    utcNtp = gmtime ( &ntpRawTime );
    uncertainty_sec = uncertainty / 1000;
    uncertainty_nsec = (uncertainty % 1000) * 1000000LL;

    ALOGI("[%s:%d]: inject time %04d/%02d/%02d %02d:%02d:%02d", __FUNCTION__,__LINE__,
                1900 + utcNtp->tm_year,
                1 + utcNtp->tm_mon,
                utcNtp->tm_mday,
                utcNtp->tm_hour,
                utcNtp->tm_min,
                utcNtp->tm_sec);

    if (gps_state != GPS_STATE_ENABLED)
    {
        ALOGE("[%s:%d]: gps_state != GPS_STATE_ENABLED, return 0", __FUNCTION__, __LINE__);
        return 0;
    }

    if (true == hadfixed)
    {
        ALOGE("[%s:%d]: fix already,not send Time, return 0", __FUNCTION__, __LINE__);
        return 0;
    }

    cmd[0] = 0xF1;
    cmd[1] = 0xD9;
    cmd[2] = 0x0B;
    cmd[3] = 0x11;

    cmd[4] = 0x00; /* UTC */
    cmd[5] = 0x00; /* reserved */
    cmd[6] = 0x12; /* leap cnt, hd8030 not used */

    cmd[7] = (1900 + utcNtp->tm_year)&0xFF;
    cmd[8] = (1900 + utcNtp->tm_year)>>8&0xFF; /* year */
    cmd[9] = 1 + utcNtp->tm_mon; /* month */
    cmd[10] = utcNtp->tm_mday; /* day */
    cmd[11] = utcNtp->tm_hour; /* hour */
    cmd[12] = utcNtp->tm_min; /* minu */
    cmd[13] = utcNtp->tm_sec; /* sec */

    cmd[18] = uncertainty_sec & 0xFF;
    cmd[19] = (uncertainty_sec >> 8) & 0xFF;
    for (int i = 0; i < 4; i++)
    {
        cmd[20 + i] = (uncertainty_nsec >> (i * 8)) & 0xFF;    
    }

    len = set_message(message, cmd, sizeof(cmd) - 4);

    ALOGI("%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            message[0],message[1],message[2],message[3],message[4],message[5],message[6],message[7],
            message[8],message[9],message[10],message[11],message[12],message[13],message[14],
            message[15],message[16],message[17],message[18],message[19],message[20],
            message[21],message[22],message[23],message[24],message[25],message[26],
            message[27],message[28],message[29]);

    if (s->fd <= 0)
    {
        ALOGE("[%s:%d]: s->fd <= 0,return -1",__FUNCTION__, __LINE__);
        return -1;
    }

    tcflush(s->fd, TCIFLUSH);
    write(s->fd, message, len);
    usleep(200 * 1000);

    ALOGI("[%s:%d]inject time success",__FUNCTION__, __LINE__);

    return 0;
}


int hdbd_gps_inject_location(double latitude, double longitude, float accuracy)

{
    unsigned char cmd[21] = {0}; /* packet head+payload */
    int my_lat = (int)(latitude   * 10000000);
    int my_long = (int)(longitude * 10000000);
    unsigned int my_acc = (unsigned int)(fabs(accuracy));
	unsigned int i = 0;
    unsigned char message[25] = {0}; /* payload:17 + packet pad:8 */

	memset(cmd, 0, sizeof(cmd));
	cmd[0] = 0xF1;
	cmd[1] = 0xD9;
	cmd[2] = 0x0B;
	cmd[3] = 0x10;

	/* LLA : 1 */
	cmd[4] = 0x01;

	/* lat : 4 */
	for (i = 0; i < 4; i++)
	{
		cmd[5 + i] = (my_lat >> (i * 8)) & 0xff;
	}

	/* long : 4 */
	for (i = 0; i < 4; i++)
	{
		cmd[9 + i] = (my_long >> (i * 8)) & 0xff;
	}

	/* alt : 4 */
	for (i = 0; i < 4; i++)
	{
		cmd[13 + i] = 0;
	}

	/* accuracy : 4 */
	for (i = 0; i < 4; i++)
	{
		cmd[17 + i] = (my_acc >> (i * 8)) & 0xff;
	}

	len = set_message(message, cmd, sizeof(cmd) - 4);

	ALOGI("%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
				message[0],message[1],message[2],message[3],message[4],message[5],message[6],message[7],
				message[8],message[9],message[10],message[11],message[12],message[13],message[14],
				message[15],message[16],message[17],message[18],message[19],message[20],
				message[21],message[22],message[23],message[24]);

}
