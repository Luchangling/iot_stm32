
#include "spi_flash_board_config.h"
#include "board.h"
#include "wx25_spi_flash_reg.h"
u16 SPI_FLASH_TYPE=0;

#define	SPI_FLASH_CS PAout(4)  //片选信号	

#define SPI_DEV SPI1

//extern void delay_us(void);

void SPI_Flash_Erase_Sector(u32 Dst_Addr);
void SPI_Flash_Wait_Busy(void);   
u16 SPI_Flash_ReadID(void);



//SPI 速度设置函数
//SpeedSet:
//SPI_BaudRatePrescaler_2   2分频   
//SPI_BaudRatePrescaler_8   8分频   
//SPI_BaudRatePrescaler_16  16分频  
//SPI_BaudRatePrescaler_256 256分频 
  
void SPI_SetSpeed(u8 SPI_BaudRatePrescaler)
{
  	assert_param(IS_SPI_BAUDRATE_PRESCALER(SPI_BaudRatePrescaler));
	SPI_DEV->CR1&=0XFFC7;
	SPI_DEV->CR1|=SPI_BaudRatePrescaler;	//设置SPI1速度 
	SPI_Cmd(SPI_DEV,ENABLE); 

} 

//SPIx 读写一个字节
//TxData:要写入的字节
//返回值:读取到的字节
u8 SPI_ReadWriteByte(u8 TxData)
{		
	u8 retry=0;				 	
	while (SPI_I2S_GetFlagStatus(SPI_DEV, SPI_I2S_FLAG_TXE) == RESET) //检查指定的SPI标志位设置与否:发送缓存空标志位
	{
    	retry++;
    	if(retry>200)return 0;
	}			  
	SPI_I2S_SendData(SPI_DEV, TxData); //通过外设SPIx发送一个数据
	retry=0;

	while (SPI_I2S_GetFlagStatus(SPI_DEV, SPI_I2S_FLAG_RXNE) == RESET) //检查指定的SPI标志位设置与否:接受缓存非空标志位
	{
    	retry++;
    	if(retry>200)return 0;
	}	  						    
	return SPI_I2S_ReceiveData(SPI_DEV); //返回通过SPIx最近接收的数据					    
}


void spi_flash_init(void)
{ 
    //GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef  SPI_InitStructure;

    RCC_APB2PeriphClockCmd( RCC_APB2Periph_SPI1,  ENABLE );//SPI1时钟使能   
    //MOSI
    gpio_type_set(PA7,GPIO_Mode_AF_PP);
    //CLK
    gpio_type_set(PA5,GPIO_Mode_AF_PP);
    gpio_out(PA5,1);
    //MISO   
    gpio_type_set(PA6,GPIO_Mode_IPU);

    //#CS 片选信号
    gpio_type_set(PA4,GPIO_Mode_Out_OD);
    gpio_out(PA4,1);
    
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;  //设置SPI单向或者双向的数据模式:SPI设置为双线双向全双工
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;       //设置SPI工作模式:设置为主SPI
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;       //设置SPI的数据大小:SPI发送接收8位帧结构
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;     //串行同步时钟的空闲状态为高电平
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;    //串行同步时钟的第二个跳变沿（上升或下降）数据被采样
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;       //NSS信号由硬件（NSS管脚）还是软件（使用SSI位）管理:内部NSS信号有SSI位控制
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_256;        //定义波特率预分频的值:波特率预分频值为256
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;  //指定数据传输从MSB位还是LSB位开始:数据传输从MSB位开始
    SPI_InitStructure.SPI_CRCPolynomial = 7;    //CRC值计算的多项式
    SPI_Init(SPI_DEV, &SPI_InitStructure);  //根据SPI_InitStruct中指定的参数初始化外设SPIx寄存器
 
    SPI_Cmd(SPI_DEV, ENABLE); //使能SPI外设

    delay_us(200);

    //SPI_FLASH_CS=1; 
    
    SPI_ReadWriteByte(0xff);//启动传输      

    SPI_SetSpeed(SPI_BaudRatePrescaler_2);//设置为18M时钟,高速模式
    //GPIO_ResetBits(GPIOA,GPIO_Pin_4);
    SPI_FLASH_TYPE=SPI_Flash_ReadID();//读取FLASH ID.  
    //GPIO_SetBits(GPIOA,GPIO_Pin_4);
  
 

}


//读取SPI_FLASH的状态寄存器
//BIT7  6   5   4   3   2   1   0
//SPR   RV  TB BP2 BP1 BP0 WEL BUSY
//SPR:默认0,状态寄存器保护位,配合WP使用
//TB,BP2,BP1,BP0:FLASH区域写保护设置
//WEL:写使能锁定
//BUSY:忙标记位(1,忙;0,空闲)
//默认:0x00
u8 SPI_Flash_ReadSR(void)   
{  
	u8 byte=0;   
	SPI_FLASH_CS=0;                            //使能器件   
	SPI_ReadWriteByte(W25X_ReadStatusReg);    //发送读取状态寄存器命令    
	byte=SPI_ReadWriteByte(0Xff);             //读取一个字节  
	SPI_FLASH_CS=1;                            //取消片选     
	return byte;   
} 
//写SPI_FLASH状态寄存器
//只有SPR,TB,BP2,BP1,BP0(bit 7,5,4,3,2)可以写!!!
void SPI_FLASH_Write_SR(u8 sr)   
{   
	SPI_FLASH_CS=0;                            //使能器件   
	SPI_ReadWriteByte(W25X_WriteStatusReg);   //发送写取状态寄存器命令    
	SPI_ReadWriteByte(sr);               //写入一个字节  
	SPI_FLASH_CS=1;                            //取消片选     	      
}   
//SPI_FLASH写使能	
//将WEL置位   
void SPI_FLASH_Write_Enable(void)   
{
	SPI_FLASH_CS=0;                            //使能器件   
    SPI_ReadWriteByte(W25X_WriteEnable);      //发送写使能  
	SPI_FLASH_CS=1;                            //取消片选     	      
} 
//SPI_FLASH写禁止	
//将WEL清零  
void SPI_FLASH_Write_Disable(void)   
{  
	SPI_FLASH_CS=0;                            //使能器件   
    SPI_ReadWriteByte(W25X_WriteDisable);     //发送写禁止指令    
	SPI_FLASH_CS=1;                            //取消片选     	      
} 			    
//读取芯片ID W25X16的ID:0XEF14
u16 SPI_Flash_ReadID(void)
{
	u16 Temp = 0;	  
	SPI_FLASH_CS=0;				    
	SPI_ReadWriteByte(0x90);//发送读取ID命令	    
	SPI_ReadWriteByte(0x00); 	    
	SPI_ReadWriteByte(0x00); 	    
	SPI_ReadWriteByte(0x00); 	 			   
	Temp|=SPI_ReadWriteByte(0xFF)<<8;  
	Temp|=SPI_ReadWriteByte(0xFF);	 
	SPI_FLASH_CS=1;				    
	return Temp;
}   		    
//读取SPI FLASH  
//在指定地址开始读取指定长度的数据
//pBuffer:数据存储区
//ReadAddr:开始读取的地址(24bit)
//NumByteToRead:要读取的字节数(最大65535)
void SPI_Flash_Read(u8* pBuffer,u32 ReadAddr,u16 NumByteToRead)   
{ 
 	u16 i;    												    
	SPI_FLASH_CS=0;                            //使能器件   
    SPI_ReadWriteByte(W25X_ReadData);         //发送读取命令   
    SPI_ReadWriteByte((u8)((ReadAddr)>>16));  //发送24bit地址    
    SPI_ReadWriteByte((u8)((ReadAddr)>>8));   
    SPI_ReadWriteByte((u8)ReadAddr);   
    for(i=0;i<NumByteToRead;i++)
	{ 
        pBuffer[i]=SPI_ReadWriteByte(0XFF);   //循环读数  
    }
	SPI_FLASH_CS=1;                            //取消片选     	      
}  
//SPI在一页(0~65535)内写入少于256个字节的数据
//在指定地址开始写入最大256字节的数据
//pBuffer:数据存储区
//WriteAddr:开始写入的地址(24bit)
//NumByteToWrite:要写入的字节数(最大256),该数不应该超过该页的剩余字节数!!!	 
void SPI_Flash_Write_Page(u8* pBuffer,u32 WriteAddr,u16 NumByteToWrite)
{
 	u16 i;  
    SPI_FLASH_Write_Enable();                  //SET WEL 
	SPI_FLASH_CS=0;                            //使能器件   
    SPI_ReadWriteByte(W25X_PageProgram);      //发送写页命令   
    SPI_ReadWriteByte((u8)((WriteAddr)>>16)); //发送24bit地址    
    SPI_ReadWriteByte((u8)((WriteAddr)>>8));   
    SPI_ReadWriteByte((u8)WriteAddr);   
    for(i=0;i<NumByteToWrite;i++)SPI_ReadWriteByte(pBuffer[i]);//循环写数  
	SPI_FLASH_CS=1;                            //取消片选 
	SPI_Flash_Wait_Busy();					   //等待写入结束
} 
//无检验写SPI FLASH 
//必须确保所写的地址范围内的数据全部为0XFF,否则在非0XFF处写入的数据将失败!
//具有自动换页功能 
//在指定地址开始写入指定长度的数据,但是要确保地址不越界!
//pBuffer:数据存储区
//WriteAddr:开始写入的地址(24bit)
//NumByteToWrite:要写入的字节数(最大65535)
//CHECK OK
void SPI_Flash_Write_NoCheck(u8* pBuffer,u32 WriteAddr,u16 NumByteToWrite)   
{ 			 		 
	u16 pageremain;	   
	pageremain=256-WriteAddr%256; //单页剩余的字节数		 	    
	if(NumByteToWrite<=pageremain)pageremain=NumByteToWrite;//不大于256个字节
	while(1)
	{	   
		SPI_Flash_Write_Page(pBuffer,WriteAddr,pageremain);
		if(NumByteToWrite==pageremain)break;//写入结束了
	 	else //NumByteToWrite>pageremain
		{
			pBuffer+=pageremain;
			WriteAddr+=pageremain;	

			NumByteToWrite-=pageremain;			  //减去已经写入了的字节数
			if(NumByteToWrite>256)pageremain=256; //一次可以写入256个字节
			else pageremain=NumByteToWrite; 	  //不够256个字节了
		}
	};	    
} 
//写SPI FLASH  
//在指定地址开始写入指定长度的数据
//该函数带擦除操作!
//pBuffer:数据存储区
//WriteAddr:开始写入的地址(24bit)
//NumByteToWrite:要写入的字节数(最大65535)  		   
u8 SPI_FLASH_BUF[4096];
void SPI_Flash_Write(u8* pBuffer,u32 WriteAddr,u16 NumByteToWrite)   
{ 
	u32 secpos;
	u16 secoff;
	u16 secremain;	   
 	u16 i;    

	secpos=WriteAddr/4096;//扇区地址 0~511 for w25x16
	secoff=WriteAddr%4096;//在扇区内的偏移
	secremain=4096-secoff;//扇区剩余空间大小   

	if(NumByteToWrite<=secremain)secremain=NumByteToWrite;//不大于4096个字节
	while(1) 
	{	
		SPI_Flash_Read(SPI_FLASH_BUF,secpos*4096,4096);//读出整个扇区的内容
		for(i=0;i<secremain;i++)//校验数据
		{
			if(SPI_FLASH_BUF[secoff+i]!=0XFF)break;//需要擦除  	  
		}
		if(i<secremain)//需要擦除
		{
			SPI_Flash_Erase_Sector(secpos);//擦除这个扇区
			for(i=0;i<secremain;i++)	   //复制
			{
				SPI_FLASH_BUF[i+secoff]=pBuffer[i];	  
			}
			SPI_Flash_Write_NoCheck(SPI_FLASH_BUF,secpos*4096,4096);//写入整个扇区  

		}else SPI_Flash_Write_NoCheck(pBuffer,WriteAddr,secremain);//写已经擦除了的,直接写入扇区剩余区间. 				   
		if(NumByteToWrite==secremain)break;//写入结束了
		else//写入未结束
		{
			secpos++;//扇区地址增1
			secoff=0;//偏移位置为0 	 

		   	pBuffer+=secremain;  //指针偏移
			WriteAddr+=secremain;//写地址偏移	   
		   	NumByteToWrite-=secremain;				//字节数递减
			if(NumByteToWrite>4096)secremain=4096;	//下一个扇区还是写不完
			else secremain=NumByteToWrite;			//下一个扇区可以写完了
		}	 
	};	 	 
}
//擦除整个芯片
//整片擦除时间:
//W25X16:25s 
//W25X32:40s 
//W25X64:40s 
//等待时间超长...
void SPI_Flash_Erase_Chip(void)   
{                                             
    SPI_FLASH_Write_Enable();                  //SET WEL 
    SPI_Flash_Wait_Busy();   
  	SPI_FLASH_CS=0;                            //使能器件   
    SPI_ReadWriteByte(W25X_ChipErase);        //发送片擦除命令  
	SPI_FLASH_CS=1;                            //取消片选     	      
	SPI_Flash_Wait_Busy();   				   //等待芯片擦除结束
}   
//擦除一个扇区
//Dst_Addr:扇区地址 0~511 for w25x16
//擦除一个山区的最少时间:150ms
void SPI_Flash_Erase_Sector(u32 Dst_Addr)   
{   
	Dst_Addr*=4096;
    SPI_FLASH_Write_Enable();                  //SET WEL 	 
    SPI_Flash_Wait_Busy();   
  	SPI_FLASH_CS=0;                            //使能器件   
    SPI_ReadWriteByte(W25X_SectorErase);      //发送扇区擦除指令 
    SPI_ReadWriteByte((u8)((Dst_Addr)>>16));  //发送24bit地址    
    SPI_ReadWriteByte((u8)((Dst_Addr)>>8));   
    SPI_ReadWriteByte((u8)Dst_Addr);  
	SPI_FLASH_CS=1;                            //取消片选     	      
    SPI_Flash_Wait_Busy();   				   //等待擦除完成
}  
//等待空闲
void SPI_Flash_Wait_Busy(void)   
{   
	while ((SPI_Flash_ReadSR()&0x01)==0x01);   // 等待BUSY位清空
}  
//进入掉电模式
void SPI_Flash_PowerDown(void)   
{ 
  	SPI_FLASH_CS=0;                            //使能器件   
    SPI_ReadWriteByte(W25X_PowerDown);        //发送掉电命令  
	SPI_FLASH_CS=1;                            //取消片选     	      
    delay_us(3);                               //等待TPD  
}   
//唤醒
void SPI_Flash_WAKEUP(void)   
{  
  	SPI_FLASH_CS=0;                            //使能器件   
    SPI_ReadWriteByte(W25X_ReleasePowerDown);   //  send W25X_PowerDown command 0xAB    
	SPI_FLASH_CS=1;                            //取消片选     	      
    delay_us(3);                               //等待TRES1
}   



