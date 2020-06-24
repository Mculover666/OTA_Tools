#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "stdint.h"
#include "math.h"

#include <QMessageBox>


#define OTA_PARTITION_MAGIC                 0x6420

typedef enum ota_update_type_en {
    OTA_UPDATE_IN_POSITION,
    OTA_UPDATE_PING_PONG,
} ota_updt_type_t;

typedef struct ota_image_version_st {
    uint8_t     major;  /* major version number */
    uint8_t     minor;  /* minor version number */
} ota_img_vs_t;

typedef struct ota_partition_st {
    uint32_t        start;          /* start address */
    uint32_t        end;            /* end address */
} ota_pt_t;

typedef struct ota_in_position_partitions_st {
    ota_pt_t    active_app;
    ota_pt_t    ota;
    ota_pt_t    kv;
} ota_ip_pts_t;

typedef struct ota_ping_pong_partitions_st {
    ota_pt_t    active_app;
    ota_pt_t    backup_app;
    ota_pt_t    ota;
    ota_pt_t    kv;
} ota_pp_pts_t;

typedef union ip_un {
    ota_pt_t        pts[sizeof(ota_ip_pts_t) / sizeof(ota_pt_t)];
    ota_ip_pts_t    ip_pts;
} ip_u;

typedef union pp_un {
    ota_pt_t        pts[sizeof(ota_pp_pts_t) / sizeof(ota_pt_t)];
    ota_pp_pts_t    pp_pts;
} pp_u;

typedef union ota_partitions_un {
    ip_u            ip;
    pp_u            pp;
} ota_pts_t;

typedef struct partition_param_st {
    ota_updt_type_t     updt_type;

    ota_img_vs_t        version;
    ota_pts_t           pts;
} partition_param_t;

typedef struct ota_partition_header_st {
    uint16_t        magic;      /* a certain number */
    ota_img_vs_t    version;    /* the initial version of the application */
    uint8_t         crc;        /* crc of the whole partition table */
    uint8_t         reserved[3];
} ota_pt_hdr_t;

static uint8_t crc8(uint8_t crc, uint8_t *buf, int nbyte)
{
    int i;

#define POLY            0x31
#define WIDTH           8
#define TOP_BIT         0x80

    while (nbyte--) {
        crc ^= *buf++;
        for (i = 0; i < WIDTH; ++i) {
            if (crc & TOP_BIT) {
                crc = (crc << 1) ^ POLY;
            } else {
                crc <<= 1;
            }
        }
        crc &= 0xFF;
    }

    return crc;
}

static uint8_t partition_hdr_crc(ota_pt_hdr_t *pt_hdr)
{
    uint8_t crc = 0;

    crc = crc8(crc, (uint8_t *)&pt_hdr->magic, sizeof(uint16_t));
    crc = crc8(crc, (uint8_t *)&pt_hdr->version, sizeof(ota_img_vs_t));
    return crc;
}

static uint8_t partitions_crc(uint8_t crc, ota_pt_t *pts, int n)
{
    return crc8(crc, (uint8_t *)pts, n * sizeof(ota_pt_t));
}

static int partitions_verify(ota_pt_t *pts, int n)
{
    int i = 0;

    for (i = 0; i < n; ++i) {
        if (pts[i].start == 0 || pts[i].start == (uint32_t)-1 ||
            pts[i].end == 0 || pts[i].end == (uint32_t)-1) {
            return -1;
        }

        if (pts[i].start >= pts[i].end) {
            return -1;
        }
    }

    return 0;
}

void write_partition(partition_param_t *param, FILE *fp)
{
    uint8_t crc = 0;
    ota_pt_hdr_t hdr;

    hdr.magic   = OTA_PARTITION_MAGIC;
    hdr.version = param->version;

    crc         = partition_hdr_crc(&hdr);

    if (param->updt_type == OTA_UPDATE_IN_POSITION) {
        if (partitions_verify(&param->pts.ip.pts[0], sizeof(param->pts.ip.pts) / sizeof(ota_pt_t)) != 0) {
            qDebug("invalid partitions\n");
        }

        crc = partitions_crc(crc, &param->pts.ip.pts[0], sizeof(param->pts.ip.pts) / sizeof(ota_pt_t));
    } else if (param->updt_type == OTA_UPDATE_PING_PONG) {
        if (partitions_verify(&param->pts.pp.pts[0], sizeof(param->pts.pp.pts) / sizeof(ota_pt_t)) != 0) {
            qDebug("invalid partitions\n");
        }

        crc = partitions_crc(crc, &param->pts.pp.pts[0], sizeof(param->pts.pp.pts) / sizeof(ota_pt_t));
    } else {
        qDebug("invalid partitions\n");
    }

    hdr.crc     = crc;

    if (fwrite(&hdr, 1, sizeof(ota_pt_hdr_t), fp) != sizeof(ota_pt_hdr_t)) {
        fclose(fp);
        qDebug("failed to write partition header\n");
    }

    if (param->updt_type == OTA_UPDATE_IN_POSITION) {
        if (fwrite(&param->pts.ip.pts[0], 1, sizeof(param->pts.ip.pts), fp) != sizeof(param->pts.ip.pts)) {
            fclose(fp);
            qDebug("failed to write partitions\n");
        }
    } else if (param->updt_type == OTA_UPDATE_PING_PONG) {
        if (fwrite(&param->pts.pp.pts[0], 1, sizeof(param->pts.pp.pts), fp) != sizeof(param->pts.pp.pts)) {
            fclose(fp);
            qDebug("failed to write partitions\n");
        }
    } else {
        qDebug("invalid partitions\n");
    }
}

uint8_t ConvertHexChar(char ch)
{
    if((ch >= '0') && (ch <= '9'))
        return ch-0x30;
    else if((ch >= 'A') && (ch <= 'F'))
        return ch-'A'+10;
    else if((ch >= 'a') && (ch <= 'f'))
        return ch-'a'+10;
}

static uint32_t hexStrToUint32(QString s)
{
    uint32_t value = 0;
    std::string str = s.toStdString();
    const char* ch = str.c_str();
    int len = s.length();
    int i = 0;

    while(len--) {
        value += ConvertHexChar(*(ch+len))*pow(16,i++);

    }

    return value;

}

void MainWindow::on_commandLinkButton_clicked()
{
    //失能生成按钮
    ui->commandLinkButton->setEnabled(false);

    qDebug("开始生成...");


    FILE *fp = NULL;
    char pt_tbl_path[] = "pt_tbl.bin";
    partition_param_t param;

    memset(&param, 0, sizeof(partition_param_t));

    /* 获取Flash起始地址 */
    //获取用户填入的十六进制字符
    QString flashStartAddrStr = ui->lineEdit->text();

    //判断
    if(flashStartAddrStr.isEmpty()) {
        //判断输入是否为空
        QMessageBox::information(NULL, "输入有误", "啊哦~你忘记输入Flash起始地址啦！",QMessageBox::No, QMessageBox::No);
        ui->commandLinkButton->setEnabled(true);
        return;
    }
    else {
        //判断输入是否是0x开头的数
        QChar ch1 = flashStartAddrStr.at( 0 );
        QChar ch2 = flashStartAddrStr.at( 1 );

        qDebug("ch1 = %c ch2 = %c", ch1, ch2);
        if(ch1 == '0' && ( ch2 == 'x' || ch2 == 'X')) {

        }
        else {
            QMessageBox::information(NULL, "输入有误", "啊哦~请输入0x开头的十六进制地址！",QMessageBox::No, QMessageBox::No);
            ui->commandLinkButton->setEnabled(true);
            return;
        }
    }


    //去除字符串最开始的0x
    QString flashStartAddrHex = flashStartAddrStr.remove(0,2);
    //转化为long
    uint32_t flashStartAddr = hexStrToUint32(flashStartAddrHex);
    qDebug("flashStartAddr is:0x%08x", flashStartAddr);

    /* 获取用户输入升级类型 */
    if(ui->radioButton->isChecked())
    {
        //in-position升级方式
        param.updt_type = OTA_UPDATE_IN_POSITION;
        qDebug("otaUpdateType is:OTA_UPDATE_IN_POSITION");
    }
    else
    {
        //pingpong升级
        param.updt_type = OTA_UPDATE_PING_PONG;
        qDebug("otaUpdateType is:OTA_UPDATE_PING_PONG");
    }

    /* 获取用户输入的固件默认版本 */
    uint32_t version_major, version_minor;
    version_major = ui->spinBox_7->value();
    version_minor = ui->spinBox_6->value();
    param.version.major = version_major;
    param.version.minor = version_minor;
    qDebug("default Version is:%d.%d", version_major, version_minor);

    uint32_t start, end;

    /* 获取用户输入的bootloader分区大小 */
    uint32_t bootloaderSize = ui->spinBox->value();
    //判断单位是否是KB
    if(ui->radioButton_3->isChecked())
    {
        bootloaderSize *= 1024;
    }
    start = flashStartAddr;
    end = start + bootloaderSize;
    qDebug("bootloader  start:0x%08x    end:0x%08x", flashStartAddr, end);

    /* 获取用户输入的active_app分区大小 */
    uint32_t activeAppSize = ui->spinBox_2->value();
    //判断单位是否是KB
    if(ui->radioButton_5->isChecked())
    {
        activeAppSize *= 1024;
    }
    //根据bootloader分区大小生成active app起始地址
    start = end;
    //计算active_app结束地址
    end = start + activeAppSize;
    //打印log
    qDebug("activeApp   start:0x%08x    end:0x%08x", start, end);
    if (param.updt_type == OTA_UPDATE_IN_POSITION) {
        param.pts.ip.ip_pts.active_app.start      = start;
        param.pts.ip.ip_pts.active_app.end        = end;
    } else if (param.updt_type == OTA_UPDATE_PING_PONG) {
        param.pts.pp.pp_pts.active_app.start      = start;
        param.pts.pp.pp_pts.active_app.end        = end;
    }

    /* 获取用户输入的backup_app分区大小 */
    if (param.updt_type == OTA_UPDATE_PING_PONG) {

        uint32_t backupAppSize = ui->spinBox_3->value();
        //判断单位是否是KB
        if(ui->radioButton_7->isChecked())
        {
            backupAppSize *= 1024;
        }
        start = end;
        end =start +backupAppSize;
        param.pts.pp.pp_pts.backup_app.start      = start;
        param.pts.pp.pp_pts.backup_app.end        = end;
        qDebug("backup App   start:0x%08x    end:0x%08x", start, end);
    }

    /* 获取用户输入的ota分区大小 */
    uint32_t otaSize = ui->spinBox_4->value();
    //判断单位是否是KB
    if(ui->radioButton_9->isChecked())
    {
        otaSize *= 1024;
    }
    start = end;
    end = start + otaSize;
    if (param.updt_type == OTA_UPDATE_IN_POSITION) {
        param.pts.ip.ip_pts.ota.start      = start;
        param.pts.ip.ip_pts.ota.end        = end;
    } else if (param.updt_type == OTA_UPDATE_PING_PONG) {
        param.pts.pp.pp_pts.ota.start      = start;
        param.pts.pp.pp_pts.ota.end        = end;
    }
    //打印log
    qDebug("ota download    start:0x%08x    end:0x%08x", start, end);

    /* 获取用户输入的ota分区大小 */
    uint32_t kvSize = ui->spinBox_5->value();
    //判断单位是否是KB
    if(ui->radioButton_11->isChecked())
    {
        kvSize *= 1024;
    }
    start = end;
    end = start + kvSize;
    if (param.updt_type == OTA_UPDATE_IN_POSITION) {
        param.pts.ip.ip_pts.kv.start      = start;
        param.pts.ip.ip_pts.kv.end        = end;
    } else if (param.updt_type == OTA_UPDATE_PING_PONG) {
        param.pts.pp.pp_pts.kv.start      = start;
        param.pts.pp.pp_pts.kv.end        = end;
    }
    //打印log
    qDebug("kv  start:0x%08x    end:0x%08x", start, end);


    /* 创建文件 */
    if ((fp = fopen(pt_tbl_path, "wb")) == NULL) {
        /* 创建文件失败，告警 */
        qDebug("failed to open: %s\n", pt_tbl_path);
    }

    /* 向文件中写入分区表信息*/
    write_partition(&param, fp);

    fclose(fp);

    /* 生成成功，使能生成按钮 */
    qDebug("生成成功!");
    QMessageBox::information(NULL, "生成成功", "文件已放到程序运行目录下啦!(pt_tbl.bin)",QMessageBox::Ok, QMessageBox::Ok);
    ui->commandLinkButton->setEnabled(true);

    /* 计算分区表推荐烧录地址 */
    uint32_t recommandAddr = end;
    char recommandAddrStr[10];
    sprintf(recommandAddrStr, "0x%08x", recommandAddr);
    ui->lineEdit_2->setText(recommandAddrStr);
}
