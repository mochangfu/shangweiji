#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QLocalSocket>
#include <QMessageBox>
#include <QUdpSocket>
#include <QThread>
#include <syncstream>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow;

class UdpReceiverThread : public QThread
{
    Q_OBJECT
public:
    explicit UdpReceiverThread(QObject *parent = nullptr) : QThread(parent), m_isRunning(true), m_isTiaozhi(false) {};
    ~UdpReceiverThread() {};
    void dealDataFromFPGA(const QByteArray &data);

    void stopThread()
    {
        m_isRunning = false;
    }
    void setInput(qint16 port , QString file, QUdpSocket *udpSocket)
    {
        m_port = port;
        m_file = file;
        mUdpSocket = udpSocket;
    }
    void setShanYInput(float BLMum, float MuBiao, float PianCha)
    {
        m_isTiaozhi = true;
        m_ShanYBL = BLMum;
        m_ShanYMuBiao = MuBiao;
        m_ShanYPianCha = PianCha;
    }
    void setWLFlag(bool wl0, bool wl1, bool wl2, bool wl3)
    {
        m_iswl0FlagON = wl0;
        m_iswl1FlagON = wl1;
        m_iswl2FlagON = wl2;
        m_iswl3FlagON = wl3;
    }
    void setWLCount0(bool wl0)
    {
        m_iswl0FlagCount = wl0;
    }
    void setWLCount1(bool wl1)
    {
        m_iswl1FlagCount = wl1;
    }
    void setWLCount2(bool wl2)
    {
        m_iswl2FlagCount = wl2;
    }
    void setWLCount3(bool wl3)
    {
        m_iswl3FlagCount = wl3;
    }
    void setIsNeedDeal(bool flag)
    {
        m_isNeedDeal = flag;
    }
    void CheckCanSendData(bool flag)
    {
        m_isNeedDeal = flag;
    }
    bool judgeQueryDateNeedSend()
    {
        bool ret = false;
        if (m_iswl0FlagON && m_iswl0FlagCount) {
            return ret;
        }
        if (m_iswl1FlagON && m_iswl1FlagCount) {
            return ret;
        }
        if (m_iswl2FlagON && m_iswl2FlagCount) {
            return ret;
        }
        if (m_iswl3FlagON && m_iswl3FlagCount) {
            return ret;
        }
        return true;
    }
    void setQueryDateNeedSend()
    {
        if (m_iswl0FlagON) {
            m_iswl0FlagCount = true;
        }
        if (m_iswl1FlagON) {
            m_iswl1FlagCount = true;
        }
        if (m_iswl2FlagON) {
            m_iswl2FlagCount = true;
        }
        if (m_iswl3FlagON) {
            m_iswl3FlagCount = true;
        }
        m_isNeedDeal = true;
        m_iswl0DateCatch = false;
        m_iswl1DateCatch = false;
        m_iswl2DateCatch = false;
        m_iswl3DateCatch = false;
        m_break = false;
    }
    bool judgeRecDateNeedCarryON()
    {
        bool ret = true;
        if (m_iswl0FlagON && m_iswl0FlagCount) {
            return ret;
        }
        if (m_iswl1FlagON && m_iswl1FlagCount) {
            return ret;
        }
        if (m_iswl2FlagON && m_iswl2FlagCount) {
            return ret;
        }
        if (m_iswl3FlagON && m_iswl3FlagCount) {
            return ret;
        }
        return false;
    }
    void judegeAndsetRecDate()
    {
        if (m_iswl0FlagCount && m_iswl0DateCatch) {
            m_iswl0FlagCount = false;
        }
        if (m_iswl1FlagCount && m_iswl1DateCatch) {
            m_iswl1FlagCount = false;
        }
        if (m_iswl2FlagCount && m_iswl2DateCatch) {
            m_iswl2FlagCount = false;
        }
        if (m_iswl3FlagCount && m_iswl3DateCatch) {
            m_iswl3FlagCount = false;
        }
    }
    bool getMbreak()
    {
        return m_break;
    }
    void getUI(MainWindow& a);

protected:
    void run() override;

private:
    bool m_isRunning;
    qint16 m_port;
    QString m_file;
    QUdpSocket *mUdpSocket;
    float m_ShanYBL;
    float m_ShanYMuBiao;
    float m_ShanYPianCha;
    bool m_isTiaozhi;
    bool m_iswl0FlagON;
    bool m_iswl1FlagON;
    bool m_iswl2FlagON;
    bool m_iswl3FlagON;
    bool m_iswl0FlagCount; //1表示该字段有数据需要处理，还未处理完成,控制循环执行流程
    bool m_iswl1FlagCount;
    bool m_iswl2FlagCount;
    bool m_iswl3FlagCount;
    bool m_iswl0DateCatch; //1表示该字段数据已经收到，0表述该字段数据没有收到
    bool m_iswl1DateCatch;
    bool m_iswl2DateCatch;
    bool m_iswl3DateCatch;
    bool m_isNeedDeal;
    bool m_break;
//    std::mutex mtx1;
    Ui::MainWindow *ui;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    QByteArray Deal_WAVE_CTRL0();
    QByteArray Deal_WAVE_CTRL6(bool data_0, bool data_1, bool data_2, bool data_3, bool data_4, bool data_5, bool data_6, bool data_7);
    QByteArray Deal_WAVE_CTRL6_Type(bool data_0, bool data_1, bool data_2, bool data_3, bool data_4, bool data_5, bool data_6, bool data_7, QString type);
    QByteArray Deal_WAVE_CTRL7(bool data_0, bool data_1, bool data_2, bool data_3, bool data_4, bool data_5, bool data_6, bool data_7);
    QByteArray Deal_WAVE_CTRL7_Type(bool data_0, bool data_1, bool data_2, bool data_3, bool data_4, bool data_5, bool data_6, bool data_7, QString type);
    QByteArray Deal_WAVE_CTRL0_Type(bool data_0, bool data_1, bool data_2, bool data_3, bool data_4, bool data_5, bool data_6, bool data_7, QString type);
    QByteArray Deal_GM3002_CTRL(bool data_0, bool data_1, bool data_2, bool data_3);
    QByteArray Deal_GM3002_CTRL_WL_BL(bool data_0, bool data_1, bool data_2, bool data_3, bool data_4, bool data_5, bool data_6, bool data_7, QString type);

    void SendDate(QByteArray head, QByteArray addr, QByteArray data);
    bool CreatStoreFile(QString pathNum);

private slots:

    void onErrorOccurred();

    void Read_Data();



    void on_Btn_send_clicked();

    void on_Btn_send_2_clicked();

    void on_Btn_send_3_clicked();

    void on_Btn_send_4_clicked();

    void on_pushButton_clicked();

    void on_pushButton_3_clicked();

    void on_pushButton_4_clicked();

    void on_pushButton_5_clicked();

    void on_Btn_send_5_clicked();

private:
    Ui::MainWindow *ui;
    QTcpSocket *socket;
    int mMSgType;
    QString mNowWriteFile;
    QUdpSocket mUdpClient;
    UdpReceiverThread mUdpReceiverThread;
    friend class UdpReceiverThread;
};

#endif // MAINWINDOW_H
