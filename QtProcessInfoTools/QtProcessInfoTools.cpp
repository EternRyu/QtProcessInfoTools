#include "QtProcessInfoTools.h"

#define DEVICE_LINK_NAME L"\\\\.\\KeProcessDevice"

//控制码起始地址
#define IRP_IOCTRL_BASE 0x8000
//控制码的生成宏（设备类型，控制码，通讯方式，权限）
#define IRP_IOCTRL_CODE(i) CTL_CODE(FILE_DEVICE_UNKNOWN,IRP_IOCTRL_BASE + i,METHOD_BUFFERED,FILE_ANY_ACCESS)
//用于通信的控制码定义
#define CTL_GetProcessCount IRP_IOCTRL_CODE(0)
#define CTL_GetProcessData IRP_IOCTRL_CODE(1)

//_TCHAR 和 QString 互相转换
#ifdef UNICODE

#define QStringToTCHAR(x)     (wchar_t*) x.utf16()
#define PQStringToTCHAR(x)    (wchar_t*) x->utf16()
#define TCHARToQString(x)     QString::fromUtf16((x))
#define TCHARToQStringN(x,y)  QString::fromUtf16((x),(y))

#else

#define QStringToTCHAR(x)     x.local8Bit().constData()
#define PQStringToTCHAR(x)    x->local8Bit().constData()
#define TCHARToQString(x)     QString::fromLocal8Bit((x))
#define TCHARToQStringN(x,y)  QString::fromLocal8Bit((x),(y))

#endif

//RGB 显示qss，可以直接调用该函数将Label控件设定成警示灯的显示形式
const QString m_red_SheetStyle = "min-width: 10px; min-height: 10px;max-width:10px; max-height: 10px;border-radius: 5px; background:red";
const QString m_green_SheetStyle = "min-width: 10px; min-height: 10px;max-width:10px; max-height: 10px;border-radius: 5px; background:green";

//驱动文件
SC_HANDLE  ScHand;
SC_HANDLE  m_CreateService;
TCHAR FilePath[MAX_PATH];
LPCTSTR ServiceName = { L"ProcessDriver" };

typedef struct _PROCESS_INFO
{
    CHAR szProcessName[256];
    ULONG64 ulProcessId;
    ULONG64 ulParentProcessId;
    ULONG64 ulPeb;
    ULONG64 ulEprocess;
}PROCESS_INFO, * PPROCESS_INFO;

QtProcessInfoTools::QtProcessInfoTools(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    //初始化控件
    InitControl();


}

QtProcessInfoTools::~QtProcessInfoTools()
{}

//初始化控件
VOID QtProcessInfoTools::InitControl()
{
    //文件路径
    TCHAR CurrentPath[MAX_PATH] = {};
    GetCurrentDirectoryW(MAX_PATH, CurrentPath);
    wcscat(CurrentPath, L"\\ProcessDriver.sys");
    //wcscpy(FilePath, CurrentPath);
    swprintf_s(FilePath, MAX_PATH, L"%s", CurrentPath);
    // 添加最大化最小化 关闭按钮
    Qt::WindowFlags windowFlag = Qt::Dialog;
    windowFlag |= Qt::WindowMinimizeButtonHint;
    windowFlag |= Qt::WindowMaximizeButtonHint;
    windowFlag |= Qt::WindowCloseButtonHint;
    setWindowFlags(windowFlag);
    this->setFixedSize(this->width(), this->height());//固定对话框并关闭最大最小化按钮
    //设置开始菜单
    ui.OpenStart->setMinimumWidth(50);
    //初始化RGB灯
    ui.LabRGB->setStyleSheet(m_red_SheetStyle);
    //初始化Table
    ui.TableProcessInfo->setColumnCount(5);
    QStringList TableList;
    TableList << "进程名" << "进程ID" << "父进程ID" << "PEB" << "EPROCESS" ;
    ui.TableProcessInfo->setHorizontalHeaderLabels(TableList);
    //绑定Menu
    connect(ui.actionRunDriver, &QAction::triggered, this, &QtProcessInfoTools::DriverToRun);
    connect(ui.actionStop, &QAction::triggered, this, &QtProcessInfoTools::DriverToStop);
    //ui.TableProcessInfo->setColumnWidth(1, 20);
    //ui.TableProcessInfo->setColumnWidth(2, 20);
    //ui.TableProcessInfo->setColumnWidth(3, 30);
    return VOID();
}

//安装并运行驱动
VOID QtProcessInfoTools::DriverToRun()
{
    ScHand = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!ScHand)
    {
        return;
    }

    m_CreateService = CreateService
    (
        ScHand,
        ServiceName,//要安装的服务的名称
        ServiceName,//用户界面程序用于标识服务的显示名称
        SERVICE_ALL_ACCESS,
        SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_IGNORE,
        FilePath,//文件路径
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    );

    if (!m_CreateService)
    {
        if (GetLastError() == ERROR_SERVICE_EXISTS)
        {
            m_CreateService = OpenService(ScHand, ServiceName, SERVICE_ALL_ACCESS);
            if (m_CreateService)
            {
                goto RD;
            }
            //安装失败
            return;
        }
    }
    RD:
    //驱动安装成功
    BOOL bRet = StartService(m_CreateService, NULL, NULL);
    if (bRet == TRUE)
    {
        ui.LabRGB->setStyleSheet(m_green_SheetStyle);
        //运行成功
    }
    //CloseServiceHandle(ScHand);
    //CloseServiceHandle(m_CreateService);

    //插入信息
    InsertInfo();

    return VOID();
}

//停止驱动
VOID QtProcessInfoTools::DriverToStop()
{
    //ScHand = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    //m_CreateService = OpenService(ScHand, ServiceName, SERVICE_STOP);
    SERVICE_STATUS svcsta = { 0 };
    BOOL bRet = ControlService(m_CreateService, SERVICE_CONTROL_STOP, &svcsta);
    if (!bRet)
    {
        //停止失败
        return;
    }
    //停止成功
    //CloseServiceHandle(m_CreateService);
    //关闭驱动方式打开服务
    //m_CreateService = OpenService(ScHand, ServiceName, SERVICE_STOP | DELETE);
    if (DeleteService(m_CreateService))
    {
        //卸载成功
        ui.LabRGB->setStyleSheet(m_red_SheetStyle);
    }
    CloseServiceHandle(ScHand);
    CloseServiceHandle(m_CreateService);
    return VOID();
}

VOID QtProcessInfoTools::InsertInfo()
{
    //清空表
    ui.TableProcessInfo->clearContents();
    //链接驱动IO
    HANDLE hDriver = CreateFile(DEVICE_LINK_NAME, GENERIC_ALL, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hDriver == INVALID_HANDLE_VALUE)
    {
        //执行失败
        return;
    }
    DWORD RetLen = 0;
    DWORD64 ProcessCount = 0;
    //获取进程数量
    DeviceIoControl(hDriver, CTL_GetProcessCount, NULL, NULL, &ProcessCount, sizeof(DWORD64), &RetLen, NULL);
    PPROCESS_INFO pProcessInfo = new PROCESS_INFO[ProcessCount * sizeof(PROCESS_INFO)]{ 0 };
    ui.TableProcessInfo->setRowCount(ProcessCount);//设置行数
    //获取进程信息
    DWORD dwSize = 0;
    BOOL bRet = DeviceIoControl(hDriver, CTL_GetProcessData, NULL, 0, pProcessInfo, ProcessCount * sizeof(PROCESS_INFO), &dwSize, NULL);
    for (size_t Index = 0; Index < ProcessCount; Index++)
    {
        ui.TableProcessInfo->setItem(Index, 0, new QTableWidgetItem(pProcessInfo->szProcessName));
        //int转16进制字符串QString
        ui.TableProcessInfo->setItem(Index, 1, new QTableWidgetItem(QString::number(pProcessInfo->ulProcessId)));

        ui.TableProcessInfo->setItem(Index, 2, new QTableWidgetItem(QString::number(pProcessInfo->ulParentProcessId)));

        ui.TableProcessInfo->setItem(Index, 3, new QTableWidgetItem(QString::number(pProcessInfo->ulPeb, 16)));

        ui.TableProcessInfo->setItem(Index, 4, new QTableWidgetItem(QString::number(pProcessInfo->ulEprocess, 16)));
        pProcessInfo++;
    }
    return VOID();
}
