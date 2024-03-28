#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <QThread>
#include <synchapi.h>

struct SendMSg
{
    int pulseNum;
    int pulseWidth;
    QString WLType;
    float WLNum;
    int BLSLType;
    int BLSLNum;
};

struct SendMSg_1
{
    QString pulseNum;
    QString pulseWidth;
    QString WLType;
};

struct SendMSgForming
{
    int pulseWidth;
    QString WLType;
    float WLNum;
    int BLSLType;
    int BLSLNum;
};

struct SendMSgReadStyle1
{
    int pulseWidth;
    QString WLType;
    float WLNum;
    int BLSLType;
    int BLSLNum;
};

struct ReciveMSg_1
{
    int pulseNum;
    int pulseWidth;
    QString WLType;
    int WLNum;
    int BLSLType;
    int BLSLNum;
};

struct ReciveMSg_2
{
    QString pulseNum;
    QString pulseWidth;
    QString WLType;
};

void UdpReceiverThread::getUI(MainWindow& a){
     ui = a.ui;
}

void UdpReceiverThread::dealDataFromFPGA(const QByteArray &data)
{
    int sum = 0;
    int count = 0;

    // 检查数据长度是否合法
    if (data.size() != 66) {
        qDebug() << "数据长度不符合要求";
        return;
    }

    // 检查开头是否是 "55fa"
    if (data.left(2).toHex() == "55fa") {
        qDebug() << "开头是 '55fa'";
        m_iswl0DateCatch = true;
    }
    if (data.left(2).toHex() == "55fb") {
        qDebug() << "开头是 '55fb'";
        m_iswl1DateCatch = true;
    }
    if (data.left(2).toHex() == "55fc") {
        qDebug() << "开头是 '55fc'";
        m_iswl2DateCatch = true;
    }
    if (data.left(2).toHex() == "55fd") {
        qDebug() << "开头是 '55fd'";
        m_iswl3DateCatch = true;
    }

    // 计算平均值
    for (int i = 2; i < data.size(); i += 2) {
        sum += (uchar(data.at(i)) << 8) + uchar(data.at(i + 1));
        count++;
    }
    // 计算偏差是否在限定范围，如果在结束循环
    // 当前只能实现有一个达到目标值就退出
    if (sum/count != 0) {
        int tmp = qAbs(m_ShanYBL*1000000 / (sum/count) -  (m_ShanYMuBiao*1000000)) / 10000;
        if (tmp <= m_ShanYPianCha) {
            m_break = true;
        }
    }

    return;
}

void UdpReceiverThread::run() {
    qDebug() << "========================================================start QThread" << m_isRunning;
    if (m_isTiaozhi) {
        // 解析出SL的值计算成电流，BL/SL计算出阻值，和目标阻值进行比较。
        // SL从哪里来，是上位机主动去获取还是底层直接返回，怎么解析。
        // 根据设置的BL值的不同，SL可能有多个值，需要对每个SL计数，每个SL达到脉冲个数后进行reset操作？是不是有多个
        // SLn n是几怎么确认？是不是跟WL保持一致
        // SL的电流值如何计算？怎么计算出SL的值
        // reset操作怎么执行？
        // 还有一个问题一直没结论, 所有电压都是通过WAVE_CTRL3设置的，怎么区分WL BL SL的电压
        while (m_isRunning)
        {
            if (m_isNeedDeal && judgeRecDateNeedCarryON()) {
                if (mUdpSocket->hasPendingDatagrams())
                {
                    QByteArray datagram;
                    datagram.resize(mUdpSocket->pendingDatagramSize());
                    QHostAddress sender;
                    quint16 senderPort;

                    qint64 bytesRead = mUdpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
                    if (bytesRead > 0) {
                        QFile file(m_file);
                        if (!file.open(QIODevice::Append | QIODevice::Text)) {
                            // 文件打开失败处理
                            qDebug() << "open file error";
                            continue;
                        }
                        qDebug() << datagram.toHex();
                        file.write(datagram.toHex());
                        file.write("\n");
                    }
                    dealDataFromFPGA(datagram);
                    judegeAndsetRecDate();
                }
            }

        }
        qDebug() << "QThread finished" << m_isRunning;
    } else {
        while (m_isRunning)
        {
            if (mUdpSocket->hasPendingDatagrams())
            {
                QByteArray datagram;
                datagram.resize(mUdpSocket->pendingDatagramSize());
                QHostAddress sender;
                quint16 senderPort;

                qint64 bytesRead = mUdpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
                if (bytesRead > 0) {
                    QFile file(m_file);
                    if (!file.open(QIODevice::Append | QIODevice::Text)) {
                        // 文件打开失败处理
                        qDebug() << "open file error";
                        continue;
                    }
                    qDebug() << datagram.toHex();
                    file.write(datagram.toHex());
                    file.write("\n");
                }
            }
        }
        qDebug() << "QThread finished" << m_isRunning;
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    mMSgType = 0;
    mNowWriteFile = "";
    //初始化TcpSocket
    socket = new QTcpSocket();
    //取消原有连接
    socket->abort();
    ui->pushButton->setEnabled(false);
}

MainWindow::~MainWindow()
{
    delete this->socket;
    delete ui;
}

void MainWindow::onErrorOccurred()
{
    QMessageBox::information(this, "错误", socket->errorString(), QMessageBox::Yes);
}

void MainWindow::Read_Data()
{
    QByteArray buffer;
    //读取缓冲区数据
    buffer = socket->readAll();
    //qDebug() << buffer;

    if(!buffer.isEmpty())
    {
        QDataStream stream(buffer);
        stream.setVersion(QDataStream::Qt_5_15);

        while (!stream.atEnd()) {
            qint32 type;
            stream >> type;

            if (type == 1) {
                ReciveMSg_1 myMsg;
                stream >> myMsg.pulseNum >> myMsg.pulseWidth >> myMsg.WLType >> myMsg.WLNum >> myMsg.BLSLType >> myMsg.BLSLNum;
                qDebug() << "read from client......" << myMsg.pulseNum << myMsg.pulseWidth << myMsg.WLType << myMsg.WLNum << myMsg.BLSLType << myMsg.BLSLNum;
                QFile file(mNowWriteFile);
                if (!file.open(QIODevice::Append | QIODevice::Text)) {
                    // 文件打开失败处理
                    QMessageBox::information(this, "提示信息", "打开存储文件失败", QMessageBox::Yes);
                    return;
                }
                // 创建文本流
                QTextStream out(&file);
                // 写入内容
                out << myMsg.pulseNum << " " << myMsg.pulseWidth << " " << myMsg.WLType << " " << myMsg.WLNum << " " << myMsg.BLSLType << " " << myMsg.BLSLNum  << " " << Qt::endl;
                // 关闭文件
                file.close();
            } else if (type == 2) {
                ReciveMSg_2 myMsg;
                stream >> myMsg.pulseNum >> myMsg.pulseWidth >> myMsg.WLType;
                qDebug() << "read from client......" << myMsg.pulseNum << myMsg.pulseWidth << myMsg.WLType;
                // 处理Address数据结构
            } else if (type == 3) {
                SendMSgReadStyle1 myMsg;
                stream >> myMsg.pulseWidth >> myMsg.WLType >> myMsg.WLNum >> myMsg.BLSLType >> myMsg.BLSLNum;
                qDebug() << "read from server......" << myMsg.pulseWidth << myMsg.WLType << myMsg.WLNum << myMsg.BLSLType << myMsg.BLSLNum;
                QFile file(mNowWriteFile);
                if (!file.open(QIODevice::Append | QIODevice::Text)) {
                    // 文件打开失败处理
                    QMessageBox::information(this, "提示信息", "打开存储文件失败", QMessageBox::Yes);
                    return;
                }
                // 创建文本流
                QTextStream out(&file);
                // 写入内容
                out << myMsg.pulseWidth << " " << myMsg.WLType << " " << myMsg.WLNum << " " << myMsg.BLSLType << " " << myMsg.BLSLNum  << " " << Qt::endl;
                // 关闭文件
                file.close();
            }
        }
//        QMessageBox::information(this, "收到消息", buffer, QMessageBox::Yes);
    }
}




bool MainWindow::CreatStoreFile(QString pathNum) {
    // 首先创建存储文件
    QString dirPath = QDir::currentPath() + "/" + pathNum;
    if (!QDir(dirPath).exists()) {
        // 创建目录
        if (!QDir().mkpath(dirPath)) {
            // 创建目录失败处理
            QMessageBox::information(this, "提示信息", "创建存储路径失败", QMessageBox::Yes);
            return false;
        }
    }
    // 获取系统时间
    QDateTime currentTime = QDateTime::currentDateTime();

    // 格式化为字符串
    QString currentTimeString = currentTime.toString("yyyy-MM-dd-hh-mm-ss");
    qDebug() << currentTimeString;
    QString nowWriteFile = dirPath+ "/" + currentTimeString + ".txt";
    mNowWriteFile = nowWriteFile;
    QFile file(nowWriteFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        // 文件打开失败处理
        QMessageBox::information(this, "提示信息", "创建存储文件失败", QMessageBox::Yes);
        return false;
    }
    return true;
}
// 16bits固定指令头16‘h55d5 + 1bit固定值0 + 15bits寄存器地址 + 32bits寄存器数据
void ApeendData(char type, float data, QByteArray *array)
{
    array->append('\x55');
    array->append('\xd5');
    array->append('\x01');
    array->append('\x00');
    array->append('\x00');
    array->append(type);
    for (int i = 0; i < 2; i++) {
        char byte = ((int)(data * 4096 / 5) >> (i * 8)) & 0xFF; // 将数据按字节分割
        array->append(byte); // 存入字节数组
    }
    array->append('\x55');
    array->append('\xd5');
    array->append('\x01');
    array->append('\x02');
    array->append('\x00');
    array->append('\x00');
    array->append('\x00');
    array->append('\x02');
}

QByteArray DealCommonDate(QByteArray type, float data)
{
    QByteArray array;
    array.append("00");
    array.append(type);
//    for (int i = 0; i < 2; i++) {
//        char byte = ((int)(data * 4096 / 5) >> (i * 8)) & 0xFF; // 将数据按字节分割
//        qDebug() << "=============byte " << byte << "data " << data;
//        array.append(byte); // 存入字节数组
//    }
    QString hexString = QString::number((int)(data * 4096 / 5), 16);
    if (hexString.length() > 4) {
        qDebug() << "data out of range" << hexString << "data " << data;
    }
    for (int i = 0; i < (4 - hexString.length()); i++) {
        array.append("0");
    }
    array.append(hexString.toStdString());
    return array;
}

// 传入四个bool值，代表BL SL WL选中与否，返回相应的转换为16进制的QString
QString Deal_BSWL(bool data_0, bool data_1, bool data_2, bool data_3)
{
    int tmp = 0;
    if (data_0) {
        tmp = tmp | 0x01;
    }
    if (data_1) {
        tmp = tmp | 0x02;
    }
    if (data_2) {
        tmp = tmp | 0x04;
    }
    if (data_3) {
        tmp = tmp | 0x08;
    }
    QString hexString = QString::number(tmp, 16);
    qDebug() << "============Deal_BSWL data is" << hexString;
    return hexString;
}

// 配置寄存器 WAVE_CTRL0
// 0-3 BL0~BL3, 4-7 SL0~SL3, 7-11 WL0~WL3
QByteArray MainWindow::Deal_WAVE_CTRL0()
{
    QByteArray array;
    array.append("00000");
    array.append(Deal_BSWL(ui->checkBox->isChecked(), ui->checkBox_2->isChecked(), ui->checkBox_3->isChecked(), ui->checkBox_4->isChecked()).toStdString());
    // SL 设置全1
    array.append(Deal_BSWL(true, true, true, true).toStdString());
    array.append(Deal_BSWL(ui->checkBox_5->isChecked(), ui->checkBox_8->isChecked(), ui->checkBox_7->isChecked(), ui->checkBox_6->isChecked()).toStdString());
    return array;
}

// 配置寄存器 WAVE_CTRL0, 根据type配置SL或者BL
// 0-3 BL0~BL3, 4-7 SL0~SL3, 7-11 WL0~WL3
QByteArray MainWindow::Deal_WAVE_CTRL0_Type(bool data_0, bool data_1, bool data_2, bool data_3, bool data_4, bool data_5, bool data_6, bool data_7, QString type)
{
    QByteArray array;
    array.append("00000");
    array.append(Deal_BSWL(data_0, data_1, data_2, data_3).toStdString());
    if (type == "SL") {
        array.append(Deal_BSWL(data_4, data_5, data_6, data_7).toStdString());
        // BL 设置全1
        array.append(Deal_BSWL(true, true, true, true).toStdString());
    } else {
        // SL 设置全1
        array.append(Deal_BSWL(true, true, true, true).toStdString());
        array.append(Deal_BSWL(data_4, data_5, data_6, data_7).toStdString());
    }
    return array;
}

// 配置寄存器 WAVE_CTRL1
// WAVE_CTRL1_mode = 0
QByteArray Deal_WAVE_CTRL1(QString data)
{
    QByteArray array;
    array.append("0000");
    QString hexString = QString::number(data.toInt(), 16);
    if (hexString.length() > 4) {
        qDebug() << "WAVE_CTRL1 data out of range" << hexString << "data " << data;
    }
    for (int i = 0; i < (4 - hexString.length()); i++) {
        array.append("0");
    }
    array.append(hexString.toStdString());
    return array;
}

// 配置寄存器 WAVE_CTRL2
// 设置波形脉冲的初始脉宽
QByteArray Deal_WAVE_CTRL2(QString data)
{
    QByteArray array;
    QString hexString = QString::number((data.toInt() / 5), 16);
    if (hexString.length() > 4) {
        qDebug() << "WAVE_CTRL2 data out of range" << hexString << "data " << data;
    }
    for (int i = 0; i < (4 - hexString.length()); i++) {
        array.append("0");
    }
    array.append(hexString.toStdString());
    array.append("0000");
    return array;
}

// 配置寄存器 WAVE_CTRL3
QByteArray Deal_WAVE_CTRL3(QString data)
{
    QByteArray array;
    QString hexString = QString::number((int)(data.toFloat() * 32768 / 5), 16);
    if (hexString.length() > 4) {
        qDebug() << "WAVE_CTRL3 data out of range" << hexString << "data " << data;
    }
    for (int i = 0; i < (4 - hexString.length()); i++) {
        array.append("0");
    }
    array.append(hexString.toStdString());
    array.append("0000");
    return array;
}

// 配置寄存器 WAVE_CTRL3,增加递增电压
QByteArray Deal_WAVE_CTRL3_DI(QString data, int Num)
{
    if (Num == 0) {
        Num = 1;
    }
    QByteArray array;
    QString hexString = QString::number((int)(data.toFloat() * 32768 / 5 / Num), 16);
    if (hexString.length() > 4) {
        qDebug() << "WAVE_CTRL3 data out of range" << hexString << "data " << data;
    }
    for (int i = 0; i < (4 - hexString.length()); i++) {
        array.append("0");
    }
    array.append(hexString.toStdString());
    for (int i = 0; i < (4 - hexString.length()); i++) {
        array.append("0");
    }
    array.append(hexString.toStdString());
    return array;
}

// 配置寄存器 WAVE_CTRL3,增加递增电压
QByteArray Deal_WAVE_CTRL3_DI_MAX_MIN_NUM(QString data_min, QString data_max, int Num)
{
    if (Num == 0) {
        Num = 1;
    }
    QByteArray array;
    QString hexString = QString::number((int)(data_min.toFloat() * 32768 / 5), 16);
    if (hexString.length() > 4) {
        qDebug() << "WAVE_CTRL3 data out of range" << hexString << "data_min " << data_min;
    }
    for (int i = 0; i < (4 - hexString.length()); i++) {
        array.append("0");
    }
    array.append(hexString.toStdString());
    QString hexString_di = QString::number((int)((data_max.toFloat() - data_min.toFloat()) * 32768 / 5 / Num), 16);
    if (hexString.length() > 4) {
        qDebug() << "WAVE_CTRL3 data out of range" << hexString_di << "data_max " << data_max;
    }
    for (int i = 0; i < (4 - hexString_di.length()); i++) {
        array.append("0");
    }
    array.append(hexString_di.toStdString());
    return array;
}

// 配置寄存器 WAVE_CTRL4
QByteArray Deal_WAVE_CTRL4(QString data)
{
    QByteArray array;
    array.append("0064");
    QString hexString = QString::number((int)(data.toFloat() * 32768 / 5), 16);
    if (hexString.length() > 4) {
        qDebug() << "WAVE_CTRL4 data out of range" << hexString << "data " << data;
    }
    for (int i = 0; i < (4 - hexString.length()); i++) {
        array.append("0");
    }
    array.append(hexString.toStdString());
    return array;
}

// 配置寄存器 WAVE_CTRL5
// 0-16脉冲波形周期的递增值 16-32脉宽
QByteArray Deal_WAVE_CTRL5(QString data)
{
    QByteArray array;
    QString hexString = QString::number(((data.toInt() + data.toInt()) / 5), 16);
    if (hexString.length() > 4) {
        qDebug() << "WAVE_CTRL5 data out of range" << hexString << "data " << data;
    }
    for (int i = 0; i < (4 - hexString.length()); i++) {
        array.append("0");
    }
    array.append(hexString.toStdString());
    array.append("0000");
    return array;
}

// 配置寄存器 WAVE_CTRL6 使能信号：脉冲
// 0-3 使能 BL0~BL3 通道; 4-7 使能 SL0~SL3 通道; 8-11 使能 WL0~WL3 通道;
QByteArray MainWindow::Deal_WAVE_CTRL6(bool data_0, bool data_1, bool data_2, bool data_3, bool data_4, bool data_5, bool data_6, bool data_7)
{
    QByteArray array;
    array.append("00000");
    array.append(Deal_BSWL(data_0, data_1, data_2, data_3).toStdString());
    // SL 设置全1
    array.append(Deal_BSWL(true, true, true, true).toStdString());
    array.append(Deal_BSWL(data_4, data_5, data_6, data_7).toStdString());
    return array;
}

// 配置寄存器 WAVE_CTRL6 使能信号：脉冲, 根据type配置SL或者BL
// 0-3 使能 BL0~BL3 通道; 4-7 使能 SL0~SL3 通道; 8-11 使能 WL0~WL3 通道;
QByteArray MainWindow::Deal_WAVE_CTRL6_Type(bool data_0, bool data_1, bool data_2, bool data_3, bool data_4, bool data_5, bool data_6, bool data_7, QString type)
{
    QByteArray array;
    array.append("00000");
    array.append(Deal_BSWL(data_0, data_1, data_2, data_3).toStdString());
    // SL 设置全1
    if (type == "SL") {
        array.append(Deal_BSWL(data_4, data_5, data_6, data_7).toStdString());
        array.append(Deal_BSWL(true, true, true, true).toStdString());
    } else {
        array.append(Deal_BSWL(true, true, true, true).toStdString());
        array.append(Deal_BSWL(data_4, data_5, data_6, data_7).toStdString());
    }
    return array;
}

// 配置寄存器 WAVE_CTRL7 使能信号：电平
// 0-3 使能 BL0~BL3 通道; 4-7 使能 SL0~SL3 通道; 8-11 使能 WL0~WL3 通道;
QByteArray MainWindow::Deal_WAVE_CTRL7(bool data_0, bool data_1, bool data_2, bool data_3, bool data_4, bool data_5, bool data_6, bool data_7)
{
    QByteArray array;
    array.append("00000");
    array.append(Deal_BSWL(data_0, data_1, data_2, data_3).toStdString());
    // SL 设置全1
    array.append(Deal_BSWL(true, true, true, true).toStdString());
    array.append(Deal_BSWL(data_4, data_5, data_6, data_7).toStdString());
    return array;
}

// 配置寄存器 WAVE_CTRL7 使能信号：电平, 根据type配置SL或者BL
// 0-3 使能 BL0~BL3 通道; 4-7 使能 SL0~SL3 通道; 8-11 使能 WL0~WL3 通道;
QByteArray MainWindow::Deal_WAVE_CTRL7_Type(bool data_0, bool data_1, bool data_2, bool data_3, bool data_4, bool data_5, bool data_6, bool data_7, QString type)
{
    QByteArray array;
    array.append("00000");
    array.append(Deal_BSWL(data_0, data_1, data_2, data_3).toStdString());
    if (type == "SL") {
        array.append(Deal_BSWL(data_4, data_5, data_6, data_7).toStdString());
        // BL 设置全1
        array.append(Deal_BSWL(true, true, true, true).toStdString());
    } else {
        // SL 设置全1
        array.append(Deal_BSWL(true, true, true, true).toStdString());
        array.append(Deal_BSWL(data_4, data_5, data_6, data_7).toStdString());
    }
    return array;
}

// 设置 WL/SL/BL 的数据开关方向 设置 SL/WL/BL 是通过配置 GM3002_CTRL 寄存器实现
// 0-3 使能 SL0~SL3 通道; 4-7 使能 BL0~BL3 通道; 8-11 使能 WL0~WL3 通道;
QByteArray MainWindow::Deal_GM3002_CTRL(bool data_0, bool data_1, bool data_2, bool data_3)
{
    QByteArray array;
    array.append("00000");
    // WL 根据设置使能
    array.append(Deal_BSWL(data_0, data_1, data_2, data_3).toStdString());
    // BL 设置全0 写数据DAC
    array.append(Deal_BSWL(false, false, false, false).toStdString());
    // SL 设置全1 读数据
    array.append(Deal_BSWL(true, true, true, true).toStdString());
    return array;
}

// 设置 WL/SL/BL 的数据开关方向 设置 SL/WL/BL 是通过配置 GM3002_CTRL 寄存器实现
// 0-3 使能 SL0~SL3 通道; 4-7 使能 BL0~BL3 通道; 8-11 使能 WL0~WL3 通道;
QByteArray MainWindow::Deal_GM3002_CTRL_WL_BL(bool data_0, bool data_1, bool data_2, bool data_3, bool data_4, bool data_5, bool data_6, bool data_7, QString type)
{
    QByteArray array;
    array.append("00000");
    // WL 根据设置使能
    array.append(Deal_BSWL(data_0, data_1, data_2, data_3).toStdString());
    if (type == "BL") {
        array.append(Deal_BSWL(data_4, data_5, data_6, data_7).toStdString());
        // SL 设置全1 读数据
        array.append(Deal_BSWL(true, true, true, true).toStdString());
    } else {
        // BL 设置全1 读数据
        array.append(Deal_BSWL(true, true, true, true).toStdString());
        array.append(Deal_BSWL(data_4, data_5, data_6, data_7).toStdString());
    }

    return array;
}


void MainWindow::SendDate(QByteArray head, QByteArray addr, QByteArray data)
{
    QByteArray myByteArray;
    myByteArray.append(head);
    myByteArray.append(addr);
    myByteArray.append(data);
    qDebug() << "myByteArray" << myByteArray;
//    QUdpSocket udpClient;
    QHostAddress address(ui->lineEdit_IP->text()); // 服务端地址
    quint16 port = ui->lineEdit_Port->text().toInt(); // 服务端端口号
    //    QHostAddress localAddress("192.168.101.16");
//    quint16 localPort = ui->lineEdit_Port_2->text().toInt();
//    udpClient.bind(QHostAddress::Any, localPort);  // 本端端口号，必须指定
    QString sendData = QString(myByteArray);
    qint64 ret = mUdpClient.writeDatagram(QByteArray::fromHex(sendData.toLatin1()), address, port); // 发送字节数组
    qDebug() << "=============ret " << ret;
}

void MainWindow::on_Btn_send_2_clicked()
{
    quint16 localPort = ui->lineEdit_Port_2->text().toInt();
    mUdpClient.bind(QHostAddress::Any, localPort);
    float vspike = ui->lineEdit_517->text().toFloat();
    float vbcmp = ui->lineEdit_516->text().toFloat();
    float vwlForward = ui->lineEdit_515->text().toFloat();
    float vth = ui->lineEdit_514->text().toFloat();
    float vIbias = ui->lineEdit_518->text().toFloat();
    float leaky_Vb2 = ui->lineEdit_513->text().toFloat();
    float leaky_Vb1 = ui->lineEdit_512->text().toFloat();
    float vcm  = ui->lineEdit_519->text().toFloat();
    if (!CreatStoreFile("1")) {
        return;
    }
    ui->textBrowser->setText(mNowWriteFile);
    mUdpReceiverThread.setInput(localPort, mNowWriteFile, &mUdpClient);
    mUdpReceiverThread.start();
    ui->pushButton->setEnabled(true);
    ui->Btn_send_2->setEnabled(false);
    QByteArray head = "55d5";
    QByteArray addr = "0100";
    QByteArray addr_1 = "0102";
    QByteArray data_1 = "00000002";
    QByteArray data = DealCommonDate("00", vspike);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("01", vbcmp);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("02", vIbias);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("03", vcm);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("04", vth);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("05", leaky_Vb1);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("06", leaky_Vb2);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("07", vwlForward);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
//    设置 SL/WL/BL 方向说明
    addr = "0400";
//    data = "000d0000";
    data = Deal_GM3002_CTRL(ui->checkBox->isChecked(), ui->checkBox_2->isChecked(), ui->checkBox_3->isChecked(), ui->checkBox_4->isChecked());
    SendDate(head, addr, data);
//    有限个固定周期固定脉宽等幅的脉冲
    addr = "0701";
    data = Deal_WAVE_CTRL1(ui->lineEdit->text());
    SendDate(head, addr, data);
    addr = "0702";
//    data = "00340000";
    data = Deal_WAVE_CTRL2(ui->lineEdit_3->text());
    SendDate(head, addr, data);
    addr = "0703";
//    data = "1f400000";
    data = Deal_WAVE_CTRL3(ui->lineEdit_5->text());
    SendDate(head, addr, data);
    addr = "0704";
    data = "00640fa0";
    SendDate(head, addr, data);
    addr = "0705";
//    data = "00480000";
    data = Deal_WAVE_CTRL5(ui->lineEdit_3->text());
    SendDate(head, addr, data);
    addr = "0700";
//    data = "00000f0f";
    data = Deal_WAVE_CTRL0_Type(ui->checkBox->isChecked(), ui->checkBox_2->isChecked(), ui->checkBox_3->isChecked(), ui->checkBox_4->isChecked(), ui->checkBox_5->isChecked(), ui->checkBox_8->isChecked(), ui->checkBox_7->isChecked(), ui->checkBox_6->isChecked(), "BL");
    SendDate(head, addr, data);
    addr = "0706";
//    data = "00000f0f";
    data = Deal_WAVE_CTRL7(ui->checkBox->isChecked(), ui->checkBox_2->isChecked(), ui->checkBox_3->isChecked(), ui->checkBox_4->isChecked(), ui->checkBox_5->isChecked(), ui->checkBox_8->isChecked(), ui->checkBox_7->isChecked(), ui->checkBox_6->isChecked());
    SendDate(head, addr, data);
}


void MainWindow::on_Btn_send_3_clicked()
{
    quint16 localPort = ui->lineEdit_Port_2->text().toInt();
    mUdpClient.bind(QHostAddress::Any, localPort);
    float vspike = ui->lineEdit_517->text().toFloat();
    float vbcmp = ui->lineEdit_516->text().toFloat();
    float vwlForward = ui->lineEdit_515->text().toFloat();
    float vth = ui->lineEdit_514->text().toFloat();
    float vIbias = ui->lineEdit_518->text().toFloat();
    float leaky_Vb2 = ui->lineEdit_513->text().toFloat();
    float leaky_Vb1 = ui->lineEdit_512->text().toFloat();
    float vcm  = ui->lineEdit_519->text().toFloat();
    if (!CreatStoreFile("2")) {
        return;
    }
    ui->textBrowser_2->setText(mNowWriteFile);
    mUdpReceiverThread.setInput(localPort, mNowWriteFile, &mUdpClient);
    mUdpReceiverThread.start();
    ui->pushButton_3->setEnabled(true);
    ui->Btn_send_3->setEnabled(false);
    QByteArray head = "55d5";
    QByteArray addr = "0100";
    QByteArray addr_1 = "0102";
    QByteArray data_1 = "00000002";
    QByteArray data = DealCommonDate("00", vspike);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("01", vbcmp);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("02", vIbias);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("03", vcm);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("04", vth);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("05", leaky_Vb1);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("06", leaky_Vb2);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("07", vwlForward);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    //    设置 SL/WL/BL 方向说明
    addr = "0400";
    //    data = "000d0000";
    data = Deal_GM3002_CTRL(ui->checkBox_11->isChecked(), ui->checkBox_10->isChecked(), ui->checkBox_9->isChecked(), ui->checkBox_12->isChecked());
    SendDate(head, addr, data);
    //    有限个固定周期固定脉宽等幅的脉冲
    addr = "0701";
    data = Deal_WAVE_CTRL1(ui->comboBox_8->currentText());
    SendDate(head, addr, data);
    addr = "0702";
    data = "ffff0000";
//    data = Deal_WAVE_CTRL2(ui->lineEdit_3->text());
    SendDate(head, addr, data);
    addr = "0703";
    //    data = "1f400000";
    data = Deal_WAVE_CTRL3(ui->lineEdit_12->text());
    SendDate(head, addr, data);
    addr = "0704";
    data = "00640fa0";
    SendDate(head, addr, data);
    addr = "0705";
    data = "ffff0000";
//    data = Deal_WAVE_CTRL5(ui->lineEdit_3->text());
    SendDate(head, addr, data);
    addr = "0700";
    //    data = "00000f0f";
    data = Deal_WAVE_CTRL0_Type(ui->checkBox_11->isChecked(), ui->checkBox_10->isChecked(), ui->checkBox_9->isChecked(), ui->checkBox_12->isChecked(), ui->checkBox_25->isChecked(), ui->checkBox_27->isChecked(), ui->checkBox_26->isChecked(), ui->checkBox_28->isChecked(), "BL");
    SendDate(head, addr, data);
    addr = "0706";
    //    data = "00000f0f";
    data = Deal_WAVE_CTRL7(ui->checkBox_11->isChecked(), ui->checkBox_10->isChecked(), ui->checkBox_9->isChecked(), ui->checkBox_12->isChecked(), ui->checkBox_25->isChecked(), ui->checkBox_27->isChecked(), ui->checkBox_26->isChecked(), ui->checkBox_28->isChecked());
    SendDate(head, addr, data);
}

void MainWindow::on_Btn_send_clicked()
{
    quint16 localPort = ui->lineEdit_Port_2->text().toInt();
    mUdpClient.bind(QHostAddress::Any, localPort);
    float vspike = ui->lineEdit_517->text().toFloat();
    float vbcmp = ui->lineEdit_516->text().toFloat();
    float vwlForward = ui->lineEdit_515->text().toFloat();
    float vth = ui->lineEdit_514->text().toFloat();
    float vIbias = ui->lineEdit_518->text().toFloat();
    float leaky_Vb2 = ui->lineEdit_513->text().toFloat();
    float leaky_Vb1 = ui->lineEdit_512->text().toFloat();
    float vcm  = ui->lineEdit_519->text().toFloat();
    if (!CreatStoreFile("3")) {
        return;
    }
    ui->textBrowser_3->setText(mNowWriteFile);
    mUdpReceiverThread.setInput(localPort, mNowWriteFile, &mUdpClient);
    mUdpReceiverThread.start();
    ui->pushButton_4->setEnabled(true);
    ui->Btn_send->setEnabled(false);
    QByteArray head = "55d5";
    QByteArray addr = "0100";
    QByteArray addr_1 = "0102";
    QByteArray data_1 = "00000002";
    QByteArray data = DealCommonDate("00", vspike);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("01", vbcmp);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("02", vIbias);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("03", vcm);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("04", vth);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("05", leaky_Vb1);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("06", leaky_Vb2);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("07", vwlForward);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    //    设置 SL/WL/BL 方向说明
    addr = "0400";
    //    data = "000d0000";
    data = Deal_GM3002_CTRL_WL_BL(ui->checkBox_16->isChecked(), ui->checkBox_14->isChecked(), ui->checkBox_15->isChecked(), ui->checkBox_13->isChecked(), ui->checkBox_29->isChecked(), ui->checkBox_30->isChecked(), ui->checkBox_31->isChecked(), ui->checkBox_32->isChecked(),ui->comboBox_11->currentText());
    SendDate(head, addr, data);
    //    有限个固定周期固定脉宽等幅的脉冲
    addr = "0701";
    data = Deal_WAVE_CTRL1(ui->lineEdit_13->text());
    SendDate(head, addr, data);
    addr = "0702";
//    data = "ffff0000";
    data = Deal_WAVE_CTRL2(ui->lineEdit_15->text());
    SendDate(head, addr, data);
    addr = "0703";
    //    data = "1f400000";
    data = Deal_WAVE_CTRL3(ui->lineEdit_14->text());
    SendDate(head, addr, data);
    addr = "0704";
    data = "00640fa0";
    SendDate(head, addr, data);
    addr = "0705";
//    data = "ffff0000";
    data = Deal_WAVE_CTRL5(ui->lineEdit_15->text());
    SendDate(head, addr, data);
    addr = "0700";
    //    data = "00000f0f";
    data = Deal_WAVE_CTRL0_Type(ui->checkBox_16->isChecked(), ui->checkBox_14->isChecked(), ui->checkBox_15->isChecked(), ui->checkBox_13->isChecked(), ui->checkBox_29->isChecked(), ui->checkBox_30->isChecked(), ui->checkBox_31->isChecked(), ui->checkBox_32->isChecked(),ui->comboBox_11->currentText());
    SendDate(head, addr, data);
    addr = "0706";
    //    data = "00000f0f";
    data = Deal_WAVE_CTRL7_Type(ui->checkBox_16->isChecked(), ui->checkBox_14->isChecked(), ui->checkBox_15->isChecked(), ui->checkBox_13->isChecked(), ui->checkBox_29->isChecked(), ui->checkBox_30->isChecked(), ui->checkBox_31->isChecked(), ui->checkBox_32->isChecked(),ui->comboBox_11->currentText());
    SendDate(head, addr, data);
}


void MainWindow::on_Btn_send_4_clicked()
{
    quint16 localPort = ui->lineEdit_Port_2->text().toInt();
    mUdpClient.bind(QHostAddress::Any, localPort);
    float vspike = ui->lineEdit_517->text().toFloat();
    float vbcmp = ui->lineEdit_516->text().toFloat();
    float vwlForward = ui->lineEdit_515->text().toFloat();
    float vth = ui->lineEdit_514->text().toFloat();
    float vIbias = ui->lineEdit_518->text().toFloat();
    float leaky_Vb2 = ui->lineEdit_513->text().toFloat();
    float leaky_Vb1 = ui->lineEdit_512->text().toFloat();
    float vcm  = ui->lineEdit_519->text().toFloat();
    if (!CreatStoreFile("4")) {
        return;
    }
    ui->textBrowser_4->setText(mNowWriteFile);
    mUdpReceiverThread.setInput(localPort, mNowWriteFile, &mUdpClient);
    mUdpReceiverThread.start();
    ui->pushButton_5->setEnabled(true);
    ui->Btn_send_4->setEnabled(false);
    QByteArray head = "55d5";
    QByteArray addr = "0100";
    QByteArray addr_1 = "0102";
    QByteArray data_1 = "00000002";
    QByteArray data = DealCommonDate("00", vspike);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("01", vbcmp);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("02", vIbias);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("03", vcm);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("04", vth);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("05", leaky_Vb1);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("06", leaky_Vb2);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("07", vwlForward);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    //    设置 SL/WL/BL 方向说明
    addr = "0400";
    //    data = "000d0000";
    data = Deal_GM3002_CTRL(ui->checkBox_17->isChecked(), ui->checkBox_18->isChecked(), ui->checkBox_20->isChecked(), ui->checkBox_19->isChecked());
    SendDate(head, addr, data);
    //    有限个固定周期固定脉宽等幅的脉冲
    addr = "0701";
    data = Deal_WAVE_CTRL1(ui->lineEdit_17->text());
    SendDate(head, addr, data);
    addr = "0702";
    //    data = "ffff0000";
    data = Deal_WAVE_CTRL2(ui->lineEdit_19->text());
    SendDate(head, addr, data);
    addr = "0703";
    //    data = "1f400000";
    data = Deal_WAVE_CTRL3(ui->lineEdit_18->text());
    SendDate(head, addr, data);
    addr = "0704";
    data = "00640fa0";
    SendDate(head, addr, data);
    addr = "0705";
    //    data = "ffff0000";
    data = Deal_WAVE_CTRL5(ui->lineEdit_19->text());
    SendDate(head, addr, data);
    addr = "0700";
    //    data = "00000f0f";
    data = Deal_WAVE_CTRL0_Type(ui->checkBox_17->isChecked(), ui->checkBox_18->isChecked(), ui->checkBox_20->isChecked(), ui->checkBox_19->isChecked(), true, true, true, true, "BL");
    SendDate(head, addr, data);
    addr = "0706";
    //    data = "00000f0f";
    data = Deal_WAVE_CTRL7_Type(ui->checkBox_17->isChecked(), ui->checkBox_18->isChecked(), ui->checkBox_20->isChecked(), ui->checkBox_19->isChecked(), true, true, true, true, "BL");
    SendDate(head, addr, data);

    addr = "0700";
    //    data = "00000f0f";
    data = Deal_WAVE_CTRL0_Type(true, true, true, true, ui->checkBox_33->isChecked(), ui->checkBox_35->isChecked(), ui->checkBox_36->isChecked(), ui->checkBox_34->isChecked(), "BL");
    SendDate(head, addr, data);
    // 设置BL递增电压
    addr = "0703";
    //    data = "1f400000";
    data = Deal_WAVE_CTRL3_DI(ui->lineEdit_16->text(), ui->lineEdit_17->text().toInt());
    SendDate(head, addr, data);

    addr = "0706";
    //    data = "00000f0f";
    data = Deal_WAVE_CTRL7_Type(true, true, true, true, ui->checkBox_33->isChecked(), ui->checkBox_35->isChecked(), ui->checkBox_36->isChecked(), ui->checkBox_34->isChecked(), "BL");
    SendDate(head, addr, data);
}


void MainWindow::on_pushButton_clicked()
{
    mUdpReceiverThread.stopThread();
    ui->Btn_send_2->setEnabled(true);
    ui->pushButton->setEnabled(false);
}


void MainWindow::on_pushButton_3_clicked()
{
    mUdpReceiverThread.stopThread();
    ui->Btn_send_3->setEnabled(true);
    ui->pushButton_3->setEnabled(false);
}


void MainWindow::on_pushButton_4_clicked()
{
    ui->pushButton_4->setEnabled(false);
    ui->Btn_send->setEnabled(true);
}


void MainWindow::on_pushButton_5_clicked()
{
    ui->pushButton_5->setEnabled(false);
    ui->Btn_send_4->setEnabled(true);
}

//栅压调制
void MainWindow::on_Btn_send_5_clicked()
{
    quint16 localPort = ui->lineEdit_Port_2->text().toInt();
    mUdpClient.bind(QHostAddress::Any, localPort);
    float vspike = ui->lineEdit_517->text().toFloat();
    float vbcmp = ui->lineEdit_516->text().toFloat();
    float vwlForward = ui->lineEdit_515->text().toFloat();
    float vth = ui->lineEdit_514->text().toFloat();
    float vIbias = ui->lineEdit_518->text().toFloat();
    float leaky_Vb2 = ui->lineEdit_513->text().toFloat();
    float leaky_Vb1 = ui->lineEdit_512->text().toFloat();
    float vcm  = ui->lineEdit_519->text().toFloat();
    if (!CreatStoreFile("5")) {
        return;
    }
    ui->textBrowser_5->setText(mNowWriteFile);
    mUdpReceiverThread.setInput(localPort, mNowWriteFile, &mUdpClient);
    mUdpReceiverThread.setShanYInput(ui->lineEdit_177->text().toFloat(), ui->lineEdit_174->text().toFloat(), ui->lineEdit_179->text().toFloat());
    // 初始化线程控制参数
    mUdpReceiverThread.setWLFlag(ui->checkBox_83->isChecked(), ui->checkBox_85->isChecked(), ui->checkBox_87->isChecked(), ui->checkBox_82->isChecked());
    mUdpReceiverThread.setWLCount0(false);
    mUdpReceiverThread.setWLCount1(false);
    mUdpReceiverThread.setWLCount2(false);
    mUdpReceiverThread.setWLCount3(false);
    mUdpReceiverThread.setIsNeedDeal(false);
    mUdpReceiverThread.start();
//    ui->pushButton_5->setEnabled(true);
    ui->Btn_send_5->setEnabled(false);
    QByteArray head = "55d5";
    QByteArray addr = "0100";
    QByteArray addr_1 = "0102";
    QByteArray data_1 = "00000002";
    QByteArray data = DealCommonDate("00", vspike);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("01", vbcmp);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("02", vIbias);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("03", vcm);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("04", vth);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("05", leaky_Vb1);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("06", leaky_Vb2);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);
    data = DealCommonDate("07", vwlForward);
    SendDate(head, addr, data);
    SendDate(head, addr_1, data_1);

    //    1. 发送查询信号
    //    2. 解析处理查询结果， 对比预制值，在范围内，结束循环
    //    3. 发送波形信号
    //    4. 忽略返回信息
    //    循环1-4
    int countNum = 0;
    bool checkOrSend = false;   // 查询&发送数据标志位
    while (true) {
        if (countNum == ui->lineEdit_178->text().toInt() || mUdpReceiverThread.getMbreak()) {
            break;
        }
        if (!checkOrSend) {  // 查询
            if (!mUdpReceiverThread.judgeQueryDateNeedSend()) {
                qDebug() << "judgeQueryDateNeedSend false";
                continue;
            }
            //    设置 SL/WL/BL 方向说明
            addr = "0400";
            //    data = "000d0000";
            data = Deal_GM3002_CTRL(ui->checkBox_83->isChecked(), ui->checkBox_85->isChecked(), ui->checkBox_87->isChecked(), ui->checkBox_82->isChecked());
            SendDate(head, addr, data);
            //    有限个固定周期固定脉宽等幅的脉冲
            addr = "0701";
            data = Deal_WAVE_CTRL1(ui->lineEdit_178->text());
            SendDate(head, addr, data);
            addr = "0702";
            //    data = "ffff0000";
            data = Deal_WAVE_CTRL2(ui->lineEdit_171->text());
            SendDate(head, addr, data);
            addr = "0703";
            //    固定0.2
            data = Deal_WAVE_CTRL3("0.2");
            SendDate(head, addr, data);
            addr = "0704";
            data = "00640fa0";
            SendDate(head, addr, data);
            addr = "0705";
            //    data = "ffff0000";
            data = Deal_WAVE_CTRL5(ui->lineEdit_171->text());
            SendDate(head, addr, data);
            addr = "0700";
            //    data = "00000f0f";
            data = Deal_WAVE_CTRL0_Type(ui->checkBox_83->isChecked(), ui->checkBox_85->isChecked(), ui->checkBox_87->isChecked(), ui->checkBox_82->isChecked(), ui->checkBox_81->isChecked(), ui->checkBox_86->isChecked(), ui->checkBox_84->isChecked(), ui->checkBox_88->isChecked(), "BL");
            SendDate(head, addr, data);
            addr = "0706";
            //    data = "00000f0f";
            data = Deal_WAVE_CTRL7_Type(ui->checkBox_83->isChecked(), ui->checkBox_85->isChecked(), ui->checkBox_87->isChecked(), ui->checkBox_82->isChecked(), ui->checkBox_81->isChecked(), ui->checkBox_86->isChecked(), ui->checkBox_84->isChecked(), ui->checkBox_88->isChecked(), "BL");
            SendDate(head, addr, data);
            checkOrSend = true;
            mUdpReceiverThread.setQueryDateNeedSend();
        } else {    // 发送数据
            if (!mUdpReceiverThread.judgeQueryDateNeedSend()) {
                qDebug() << "judgeQueryDateNeedSend false";
                continue;
            }
            //    设置 SL/WL/BL 方向说明
            addr = "0400";
            //    data = "000d0000";
            data = Deal_GM3002_CTRL(ui->checkBox_83->isChecked(), ui->checkBox_85->isChecked(), ui->checkBox_87->isChecked(), ui->checkBox_82->isChecked());
            SendDate(head, addr, data);
            //    有限个固定周期固定脉宽等幅的脉冲
            addr = "0701";
            data = Deal_WAVE_CTRL1(ui->lineEdit_178->text());
            SendDate(head, addr, data);
            addr = "0702";
            //    data = "ffff0000";
            data = Deal_WAVE_CTRL2(ui->lineEdit_171->text());
            SendDate(head, addr, data);
            addr = "0703";
            //    data = "1f400000";
            QString dianya =  QString::number((ui->lineEdit_175->text().toFloat() - ui->lineEdit_167->text().toFloat())/ui->lineEdit_178->text().toInt());
            data = Deal_WAVE_CTRL3(dianya);
            SendDate(head, addr, data);
            addr = "0704";
            data = "00640fa0";
            SendDate(head, addr, data);
            addr = "0705";
            //    data = "ffff0000";
            data = Deal_WAVE_CTRL5(ui->lineEdit_171->text());
            SendDate(head, addr, data);
            addr = "0700";
            //    data = "00000f0f";
            data = Deal_WAVE_CTRL0_Type(ui->checkBox_83->isChecked(), ui->checkBox_85->isChecked(), ui->checkBox_87->isChecked(), ui->checkBox_82->isChecked(), ui->checkBox_81->isChecked(), ui->checkBox_86->isChecked(), ui->checkBox_84->isChecked(), ui->checkBox_88->isChecked(), "BL");
            SendDate(head, addr, data);
            addr = "0706";
            //    data = "00000f0f";
            data = Deal_WAVE_CTRL7_Type(ui->checkBox_83->isChecked(), ui->checkBox_85->isChecked(), ui->checkBox_87->isChecked(), ui->checkBox_82->isChecked(), ui->checkBox_81->isChecked(), ui->checkBox_86->isChecked(), ui->checkBox_84->isChecked(), ui->checkBox_88->isChecked(), "BL");
            SendDate(head, addr, data);
            countNum ++ ;
            checkOrSend = false;
            mUdpReceiverThread.CheckCanSendData(false);
        }

    }
    // set操作未成功，reset操作
    if (countNum >= ui->lineEdit_178->text().toInt()) {
        countNum = 0;
        while (true) {
            if (countNum == ui->lineEdit_168->text().toInt() || mUdpReceiverThread.getMbreak()) {
                break;
            }
            if (!checkOrSend) {  // 查询
                if (!mUdpReceiverThread.judgeQueryDateNeedSend()) {
                    qDebug() << "judgeQueryDateNeedSend false";
                    continue;
                }
                //    设置 SL/WL/BL 方向说明
                addr = "0400";
                //    data = "000d0000";
                data = Deal_GM3002_CTRL(ui->checkBox_83->isChecked(), ui->checkBox_85->isChecked(), ui->checkBox_87->isChecked(), ui->checkBox_82->isChecked());
                SendDate(head, addr, data);
                //    有限个固定周期固定脉宽等幅的脉冲
                addr = "0701";
                data = Deal_WAVE_CTRL1(ui->lineEdit_168->text());
                SendDate(head, addr, data);
                addr = "0702";
                //    data = "ffff0000";
                data = Deal_WAVE_CTRL2(ui->lineEdit_171->text());
                SendDate(head, addr, data);
                addr = "0703";
                //    固定0.2
                data = Deal_WAVE_CTRL3("0.2");
                SendDate(head, addr, data);
                addr = "0704";
                data = "00640fa0";
                SendDate(head, addr, data);
                addr = "0705";
                //    data = "ffff0000";
                data = Deal_WAVE_CTRL5(ui->lineEdit_171->text());
                SendDate(head, addr, data);
                addr = "0700";
                //    data = "00000f0f";
                data = Deal_WAVE_CTRL0_Type(ui->checkBox_83->isChecked(), ui->checkBox_85->isChecked(), ui->checkBox_87->isChecked(), ui->checkBox_82->isChecked(), ui->checkBox_81->isChecked(), ui->checkBox_86->isChecked(), ui->checkBox_84->isChecked(), ui->checkBox_88->isChecked(), "BL");
                SendDate(head, addr, data);
                addr = "0706";
                //    data = "00000f0f";
                data = Deal_WAVE_CTRL7_Type(ui->checkBox_83->isChecked(), ui->checkBox_85->isChecked(), ui->checkBox_87->isChecked(), ui->checkBox_82->isChecked(), ui->checkBox_81->isChecked(), ui->checkBox_86->isChecked(), ui->checkBox_84->isChecked(), ui->checkBox_88->isChecked(), "BL");
                SendDate(head, addr, data);
                checkOrSend = true;
                mUdpReceiverThread.setQueryDateNeedSend();
            } else {    // 发送数据
                if (!mUdpReceiverThread.judgeQueryDateNeedSend()) {
                    qDebug() << "judgeQueryDateNeedSend false";
                    continue;
                }
                //    设置 SL/WL/BL 方向说明
                addr = "0400";
                //    data = "000d0000";
                data = Deal_GM3002_CTRL(ui->checkBox_83->isChecked(), ui->checkBox_85->isChecked(), ui->checkBox_87->isChecked(), ui->checkBox_82->isChecked());
                SendDate(head, addr, data);
                //    有限个固定周期固定脉宽等幅的脉冲
                addr = "0701";
                data = Deal_WAVE_CTRL1(ui->lineEdit_168->text());
                SendDate(head, addr, data);
                addr = "0702";
                //    data = "ffff0000";
                data = Deal_WAVE_CTRL2(ui->lineEdit_171->text());
                SendDate(head, addr, data);
                addr = "0703";
                //    data = "1f400000";
                QString dianya =  QString::number((ui->lineEdit_176->text().toFloat() - ui->lineEdit_170->text().toFloat())/ui->lineEdit_168->text().toInt());
                data = Deal_WAVE_CTRL3(dianya);
                SendDate(head, addr, data);
                addr = "0704";
                data = "00640fa0";
                SendDate(head, addr, data);
                addr = "0705";
                //    data = "ffff0000";
                data = Deal_WAVE_CTRL5(ui->lineEdit_171->text());
                SendDate(head, addr, data);
                addr = "0700";
                //    data = "00000f0f";
                data = Deal_WAVE_CTRL0_Type(ui->checkBox_83->isChecked(), ui->checkBox_85->isChecked(), ui->checkBox_87->isChecked(), ui->checkBox_82->isChecked(), ui->checkBox_81->isChecked(), ui->checkBox_86->isChecked(), ui->checkBox_84->isChecked(), ui->checkBox_88->isChecked(), "BL");
                SendDate(head, addr, data);
                addr = "0706";
                //    data = "00000f0f";
                data = Deal_WAVE_CTRL7_Type(ui->checkBox_83->isChecked(), ui->checkBox_85->isChecked(), ui->checkBox_87->isChecked(), ui->checkBox_82->isChecked(), ui->checkBox_81->isChecked(), ui->checkBox_86->isChecked(), ui->checkBox_84->isChecked(), ui->checkBox_88->isChecked(), "BL");
                SendDate(head, addr, data);
                countNum ++ ;
                checkOrSend = false;
                mUdpReceiverThread.CheckCanSendData(false);
            }

        }
    }

}


