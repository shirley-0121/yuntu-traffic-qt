#include "mainwindow.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QComboBox>
#include <QDateEdit>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QFile>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QStandardPaths>
#include <QSignalBlocker>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimeEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace {

QString q(const char* value) { 
    return QString::fromUtf8(value); 
}
QLabel* title(const QString& value) { 
    auto* label = new QLabel(value); 
    label->setObjectName("pageTitle");
    return label; 
}
QPushButton* button(const QString& value, bool primary = false) { 
    auto* b = new QPushButton(value); 
    if(primary)b->setObjectName("primaryButton"); 
    return b; 
}
QWidget* card() { 
    auto* w = new QWidget;
    w->setObjectName("card"); 
    return w; 
}

QString prepareDataDirectory() {
    // 便携模式：直接读写 exe 同目录下的 data，不再复制到 AppData
    const QString dir = QDir(QCoreApplication::applicationDirPath()).filePath("data");
    QDir().mkpath(dir);
    return dir;
}

void clearPanel(QWidget* panel) {
    if (!panel || !panel->layout()) return;
    QLayout* old = panel->layout();
    while (QLayoutItem* item = old->takeAt(0)) {
        if (QWidget* widget = item->widget()) delete widget;
        if (QLayout* child = item->layout()) delete child;
        delete item;
    }
    delete old;
}

QString errorText(const std::string& value) { 
    return QString::fromUtf8(value.c_str()); 
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      store_(prepareDataDirectory().toStdString()) {
    setWindowTitle(QStringLiteral("云途智能交通预约系统"));
    resize(1180, 760);
    setMinimumSize(980, 650);
    setStyleSheet(R"(
        QWidget { font-family: "Microsoft YaHei"; color: #17324d; }
        QMainWindow { background: #f5f9fc; }
        #topBar { background: #0e7490; color: white; padding: 14px; }
        #pageTitle { font-size: 25px; font-weight: 700; color: #114b67; margin: 6px 0 12px; }
        #subtitle { color: #55738b; font-size: 14px; }
        #card { background: white; border: 1px solid #dceaf1; border-radius: 14px; padding: 18px; }
        QPushButton { background: #e8f3f6; border: 0; border-radius: 9px; padding: 10px 16px; }
        QPushButton:hover { background: #d1edf1; }
        #primaryButton { background: #16a6a4; color: white; font-weight: 700; }
        #primaryButton:hover { background: #0c8e8c; }
        QLineEdit, QComboBox, QDateEdit, QTimeEdit { background: white; border: 1px solid #c8dce5; border-radius: 8px; padding: 8px; }
        QListWidget, QTableWidget { background: white; border: 1px solid #dceaf1; border-radius: 10px; }
        QTableWidget { gridline-color: #e7f0f4; }
        QHeaderView::section { background: #eaf6f7; padding: 8px; border: 0; font-weight: 700; }
        QTabBar::tab { padding: 10px 18px; }
        QTabBar::tab:selected { color: #078b8d; font-weight: 700; border-bottom: 2px solid #16a6a4; }
    )");

    std::string error;
    if (!store_.load(error)) {
        QMessageBox::critical(this, QStringLiteral("数据加载失败"), errorText(error));
    }
    buildPages();
    activeDate_ = QString::fromStdString(transit::systemDateText());
    clockTimer_ = new QTimer(this);
    clockTimer_->setInterval(1000);
    connect(clockTimer_, &QTimer::timeout, this, &MainWindow::refreshClockAndAvailability);
    clockTimer_->start();
    refreshClockAndAvailability();
}

QString MainWindow::text(const std::string& value) const { return QString::fromUtf8(value.c_str()); }
QString MainWindow::text(const char* value) const { return QString::fromUtf8(value ? value : ""); }

//构建窗口
void MainWindow::buildPages() {
    pages_ = new QStackedWidget(this);
    setCentralWidget(pages_);
    buildWelcome(); buildPassengerLogin(); buildRegister(); buildAdminLogin();
    buildPassengerPage(); buildAdminPage(); pages_->setCurrentWidget(welcomePage_);
}

//构建选择身份窗口
void MainWindow::buildWelcome() {
    welcomePage_ = new QWidget; 
    auto* layout = new QVBoxLayout(welcomePage_); 
    layout->setContentsMargins(70, 50, 70, 50);
    auto* hero = new QWidget;
    hero->setObjectName("topBar"); 
    auto* heroLayout = new QVBoxLayout(hero);
    auto* h = new QLabel(QStringLiteral("云途智能交通预约系统")); 
    h->setStyleSheet("font-size:32px;font-weight:700;color:white;");
    auto* s = new QLabel(QStringLiteral("S · A · B · E  |  安全、清晰、可演示的预约流程")); 
    s->setStyleSheet("font-size:15px;color:#d7fbff;"); 
    heroLayout->addWidget(h);
    heroLayout->addWidget(s); 
    layout->addWidget(hero);
    layout->addSpacing(30); layout->addWidget(title(QStringLiteral("选择进入方式")));
    auto* cards = new QHBoxLayout; 
    auto* passenger = card(); 
    auto* pLayout = new QVBoxLayout(passenger); 
    pLayout->addWidget(new QLabel(QStringLiteral("乘客端")));
    pLayout->addWidget(new QLabel(QStringLiteral("查询班次、选择座位、完成预约并管理订单"))); 
    auto* pButton = button(QStringLiteral("进入乘客端"), true); 
    pLayout->addWidget(pButton); cards->addWidget(passenger);
    auto* admin = card(); 
    auto* aLayout = new QVBoxLayout(admin); 
    aLayout->addWidget(new QLabel(QStringLiteral("管理员端"))); 
    aLayout->addWidget(new QLabel(QStringLiteral("维护班次、订单、乘客账户和营收统计")));
    auto* aButton = button(QStringLiteral("进入管理员端"), true); 
    aLayout->addWidget(aButton);
    cards->addWidget(admin); layout->addLayout(cards); layout->addStretch();
    connect(pButton, &QPushButton::clicked, this, [this]{ pages_->setCurrentWidget(passengerLoginPage_); });
    connect(aButton, &QPushButton::clicked, this, [this]{ pages_->setCurrentWidget(adminLoginPage_); });
    pages_->addWidget(welcomePage_);
}

//构建乘客登录/注册窗口
void MainWindow::buildPassengerLogin() {
    passengerLoginPage_ = new QWidget; auto* outer = new QVBoxLayout(passengerLoginPage_); outer->setContentsMargins(260, 90, 260, 90); outer->addWidget(title(QStringLiteral("乘客登录")));
    auto* box = card(); auto* form = new QFormLayout(box); passengerPhone_=new QLineEdit; passengerPhone_->setPlaceholderText(QStringLiteral("11 位手机号")); passengerPassword_=new QLineEdit; passengerPassword_->setEchoMode(QLineEdit::Password); form->addRow(QStringLiteral("手机号"), passengerPhone_); form->addRow(QStringLiteral("密码"), passengerPassword_);
    auto* login=button(QStringLiteral("登录"),true); auto* reg=button(QStringLiteral("注册新账户")); auto* back=button(QStringLiteral("返回")); form->addRow(login); form->addRow(reg); form->addRow(back); outer->addWidget(box); outer->addStretch();
    connect(login,&QPushButton::clicked,this,[this]{ static int attempts=0; const auto phone=passengerPhone_->text().trimmed().toStdString(); const auto* user=store_.passengerLogin(phone,passengerPassword_->text().toStdString()); if(!user){++attempts;showError(QStringLiteral("手机号、密码错误或账户已停用，还可尝试 %1 次。").arg(qMax(0,transit::kMaxLoginAttempts-attempts)));if(attempts>=transit::kMaxLoginAttempts){attempts=0;pages_->setCurrentWidget(welcomePage_);}return;} attempts=0; currentPhone_=passengerPhone_->text().trimmed(); pages_->setCurrentWidget(passengerPage_); refreshBusList(); refreshPassengerOrders(); });
    connect(reg,&QPushButton::clicked,this,[this]{ pages_->setCurrentWidget(registerPage_); });
    connect(back,&QPushButton::clicked,this,[this]{ pages_->setCurrentWidget(welcomePage_); }); pages_->addWidget(passengerLoginPage_);
}

//乘客注册
void MainWindow::buildRegister() {
    registerPage_=new QWidget; 
    auto* outer=new QVBoxLayout(registerPage_); 
    outer->setContentsMargins(260,70,260,70); 
    outer->addWidget(title(QStringLiteral("乘客注册"))); 
    auto* box=card(); 
    auto* form=new QFormLayout(box);
    registerPhone_=new QLineEdit; 
    registerPassword_=new QLineEdit;
    registerName_=new QLineEdit; 
    registerPassword_->setEchoMode(QLineEdit::Password); 
    form->addRow(QStringLiteral("手机号"),registerPhone_);
    form->addRow(QStringLiteral("密码"),registerPassword_);
    form->addRow(QStringLiteral("姓名"),registerName_);
    auto* submit=button(QStringLiteral("注册并返回登录"),true);
    auto* back=button(QStringLiteral("返回"));
    form->addRow(submit);form->addRow(back);
    outer->addWidget(box);outer->addStretch();

    connect(submit,&QPushButton::clicked,this,[this]{std::string error;
    if(!store_.registerPassenger(registerPhone_->text().trimmed().toStdString(),registerPassword_->text().toStdString(),registerName_->text().trimmed().toUtf8().toStdString(),error)){
        showError(errorText(error));
        return;
    }
    showInfo(QStringLiteral("注册成功，请使用手机号登录。"));
    pages_->setCurrentWidget(passengerLoginPage_);});
    connect(back,&QPushButton::clicked,this,[this]{pages_->setCurrentWidget(passengerLoginPage_);});
    pages_->addWidget(registerPage_);
}

void MainWindow::buildAdminLogin() {
    adminLoginPage_=new QWidget; 
    auto* outer=new QVBoxLayout(adminLoginPage_);
    outer->setContentsMargins(260,90,260,90);
    outer->addWidget(title(QStringLiteral("管理员登录")));
    auto* box=card();
    auto* form=new QFormLayout(box);
    adminAccount_=new QLineEdit;
    adminPassword_=new QLineEdit;
    adminPassword_->setEchoMode(QLineEdit::Password);
    form->addRow(QStringLiteral("管理员账号"),adminAccount_);
    form->addRow(QStringLiteral("密码"),adminPassword_);
    auto* login=button(QStringLiteral("登录"),true);
    auto* back=button(QStringLiteral("返回"));
    form->addRow(login);
    form->addRow(back);
    outer->addWidget(box);
    outer->addStretch();
    connect(login,&QPushButton::clicked,this,[this]{static int attempts=0;const auto* admin=store_.adminLogin(adminAccount_->text().trimmed().toStdString(),adminPassword_->text().toStdString());if(!admin){++attempts;showError(QStringLiteral("登录失败，还可尝试 %1 次。" ).arg(qMax(0,transit::kMaxLoginAttempts-attempts)));if(attempts>=transit::kMaxLoginAttempts)pages_->setCurrentWidget(welcomePage_);return;}attempts=0;currentAdmin_=adminAccount_->text().trimmed();pages_->setCurrentWidget(adminPage_);resetBusDateToLatest();refreshOrderTable();refreshAccountTable();refreshRevenue();});
    connect(back,&QPushButton::clicked,this,[this]{pages_->setCurrentWidget(welcomePage_);});pages_->addWidget(adminLoginPage_);
}

//构建乘客工作台界面 包括查询，选位，购票，查看“我的车票”，退票
void MainWindow::buildPassengerPage() {
    passengerPage_=new QWidget;
    auto* outer=new QVBoxLayout(passengerPage_);
    auto* top=new QWidget;
    top->setObjectName("topBar");
    auto* topLayout=new QHBoxLayout(top);
    auto* label=new QLabel(QStringLiteral("乘客工作台"));
    label->setStyleSheet("color:white;font-size:20px;font-weight:700;");
    topLayout->addWidget(label);
    clockLabel_=new QLabel;clockLabel_->setStyleSheet("color:white;font-size:15px;");
    topLayout->addStretch();topLayout->addWidget(clockLabel_);
    auto* logout=button(QStringLiteral("退出登录"));
    topLayout->addWidget(logout);
    outer->addWidget(top);

    auto* body=new QHBoxLayout;
    auto* nav=card();
    auto* navLayout=new QVBoxLayout(nav);
    auto* queryNav=button(QStringLiteral("路线与座位"),true);
    auto* orderNav=button(QStringLiteral("我的订单"));
    navLayout->addWidget(queryNav);
    navLayout->addWidget(orderNav);
    navLayout->addStretch();
    body->addWidget(nav);
    auto* content=new QStackedWidget;
    body->addWidget(content,1);
    outer->addLayout(body);
    
    auto* query=new QWidget;
    auto* ql=new QVBoxLayout(query);
    ql->addWidget(title(QStringLiteral("查询班次并选择座位")));
    auto* filters=card();
    auto* fl=new QHBoxLayout(filters);
    direction_=new QComboBox;
    direction_->addItems({QStringLiteral("S → E"),QStringLiteral("E → S")});
    fromStop_=new QComboBox;toStop_=new QComboBox;
    for(int i=0;i<transit::kStops;++i){
        fromStop_->addItem(text(transit::stopName(i)),i);
        toStop_->addItem(text(transit::stopName(i)),i);
    }
    fromStop_->setCurrentIndex(0);
    toStop_->setCurrentIndex(3);
    auto* refresh=button(QStringLiteral("查询"),true);
    demoTime_=new QCheckBox(QStringLiteral("使用演示时间"));
    demoTime_->setChecked(false);
    demoTimeEdit_=new QTimeEdit(QTime::currentTime());
    demoTimeEdit_->setDisplayFormat(QStringLiteral("HH:mm"));
    demoTimeEdit_->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    demoTimeEdit_->setWrapping(true);
    demoTimeEdit_->setFixedWidth(96);
    fl->addWidget(new QLabel(QStringLiteral("方向")));
    fl->addWidget(direction_);
    fl->addWidget(new QLabel(QStringLiteral("出发站")));
    fl->addWidget(fromStop_);
    fl->addWidget(new QLabel(QStringLiteral("到达站")));
    fl->addWidget(toStop_);
    fl->addWidget(demoTime_);
    fl->addWidget(demoTimeEdit_);
    fl->addWidget(refresh);
    ql->addWidget(filters);
    auto* split=new QHBoxLayout;
    busList_=new QListWidget;
    busList_->setWordWrap(true);
    busList_->setSpacing(4);
    busList_->setStyleSheet("QListWidget::item { padding: 8px; }");
    busList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    split->addWidget(busList_,1);
    auto* right=card();
    auto* rl=new QVBoxLayout(right);
    querySummary_=new QLabel(QStringLiteral("请选择班次"));
    rl->addWidget(querySummary_);
    seatPanel_=new QWidget;
    seatPanel_->setMinimumHeight(360);
    rl->addWidget(seatPanel_);
    bookButton_=button(QStringLiteral("确认预约"),true);
    bookButton_->setEnabled(false);
    rl->addWidget(bookButton_);
    split->addWidget(right,2);
    ql->addLayout(split);
    content->addWidget(query);
   
    auto* orders=new QWidget;
    auto* ol=new QVBoxLayout(orders);
    ol->addWidget(title(QStringLiteral("我的订单")));
    passengerOrders_=new QTableWidget;
    passengerOrders_->setColumnCount(8);
    passengerOrders_->setHorizontalHeaderLabels({"订单号","日期","班次","路线","座位","票价","状态","操作"});
    passengerOrders_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    passengerOrders_->horizontalHeader()->setStretchLastSection(true);
    ol->addWidget(passengerOrders_);
    auto* cancel=button(QStringLiteral("取消选中订单"));
    ol->addWidget(cancel);content->addWidget(orders);
    
    connect(queryNav,&QPushButton::clicked,this,[content]{content->setCurrentIndex(0);});
    connect(orderNav,&QPushButton::clicked,this,[this,content]{content->setCurrentIndex(1);refreshPassengerOrders();});
    connect(logout,&QPushButton::clicked,this,[this]{leaveSession();});
    connect(refresh,&QPushButton::clicked,this,[this]{refreshBusList();});
    connect(demoTime_,&QCheckBox::toggled,this,[this]{refreshBusList(true);});
    connect(demoTimeEdit_,&QTimeEdit::timeChanged,this,[this]{
        if(demoTime_&&demoTime_->isChecked())refreshBusList(true);
    });
    connect(direction_,&QComboBox::currentIndexChanged,this,[this](int index){QSignalBlocker fromBlock(fromStop_),toBlock(toStop_);
    if(index==0){
        fromStop_->setCurrentIndex(0);
        toStop_->setCurrentIndex(3);
    }
    else{
        fromStop_->setCurrentIndex(3);
        toStop_->setCurrentIndex(0);
    }
    selectedBusId_=0;
    selectedRow_=-1;
    selectedCol_=-1;refreshBusList();});
    connect(fromStop_,&QComboBox::currentIndexChanged,this,[this]{selectedBusId_=0;selectedRow_=-1;selectedCol_=-1;refreshBusList();});connect(toStop_,&QComboBox::currentIndexChanged,this,[this]{selectedBusId_=0;selectedRow_=-1;selectedCol_=-1;refreshBusList();});connect(busList_,&QListWidget::currentRowChanged,this,[this]{if(busList_->currentItem()){selectedBusId_=busList_->currentItem()->data(Qt::UserRole).toInt();}else selectedBusId_=0;selectedRow_=-1;selectedCol_=-1;refreshSeatMap();});connect(bookButton_,&QPushButton::clicked,this,[this]{if(!selectedBusId_||selectedRow_<0){return;}transit::Ticket created;std::string error;if(!store_.book(selectedBusId_,currentPhone_.toStdString(),currentFrom(),currentTo(),selectedRow_,selectedCol_,effectiveNowMinutes(),created,error)){showError(errorText(error));return;}showInfo(QStringLiteral("预约成功，订单号：%1").arg(text(created.id)));refreshBusList();refreshPassengerOrders();});connect(cancel,&QPushButton::clicked,this,[this]{int row=passengerOrders_->currentRow();if(row<0)return;auto id=passengerOrders_->item(row,0)->text().toStdString();std::string error;if(!store_.cancelOrder(id,currentPhone_.toStdString(),false,error)){showError(errorText(error));return;}showInfo(QStringLiteral("订单已取消，座位已释放。"));refreshPassengerOrders();refreshBusList();});pages_->addWidget(passengerPage_);
}

// ==================== 管理员工作台（不包含登录页） ====================
/*
 * 构建管理员工作台页面。
 * 功能：搭建顶部栏和 5 个标签页（班次管理、订单管理、乘客管理、营收统计、数据维护），
 *       并为每个按钮绑定点击事件。业务规则（校验、保存）由 TransitStore 负责。
 * 参数：无。
 * 返回值：无（页面保存到成员 adminPage_）。
 */
void MainWindow::buildAdminPage() {
    // ---------- 页面骨架：顶部栏 + 标签页容器 ----------
    adminPage_ = new QWidget;                       // 管理员工作台整页容器
    auto* outer = new QVBoxLayout(adminPage_);      // 纵向布局：顶部栏在上，标签页在下
    auto* top = new QWidget;                        // 顶部栏容器
    top->setObjectName("topBar");                   // 启用全局样式表中的深色顶栏样式
    auto* tl = new QHBoxLayout(top);                // 顶部栏横向布局
    auto* label = new QLabel(QStringLiteral("管理员工作台")); // 顶栏标题
    label->setStyleSheet("color:white;font-size:20px;font-weight:700;"); // 白色加粗标题
    tl->addWidget(label);                           // 标题靠左
    tl->addStretch();                               // 弹性空白，把按钮推到最右侧
    auto* logout = button(QStringLiteral("退出登录")); // 退出登录按钮
    tl->addWidget(logout);                          // 按钮靠右
    outer->addWidget(top);                          // 顶部栏加入页面
    auto* tabs = new QTabWidget;                    // 标签页容器：每个标签页对应一个管理模块
    outer->addWidget(tabs);                         // 标签页加入页面主体
    // 点击“退出登录”：清空当前会话并返回欢迎页
    connect(logout, &QPushButton::clicked, this, [this] { leaveSession(); });
    // ==================== 标签页 1：班次管理 ====================
    auto* buses = new QWidget;                       // 班次管理页容器
    auto* bl = new QVBoxLayout(buses);               // 纵向布局：日期行 → 表格 → 按钮行
    busTable_ = new QTableWidget;                    // 班次表格（只读）
    busTable_->setColumnCount(6);                    // 6 列
    busTable_->setHorizontalHeaderLabels({"班次", "发车时间", "方向", "状态",
                                          "有效订单", "区间可用座位"}); // 表头
    busTable_->setSelectionBehavior(QAbstractItemView::SelectRows);    // 点击单元格时整行选中
    busTable_->setSelectionMode(QAbstractItemView::SingleSelection);   // 一次只能选一个班次
    busTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);     // 只读：禁止双击修改单元格
    busTable_->horizontalHeader()->setStretchLastSection(true);        // 最后一列占满剩余宽度

    // ---------- 订单日期筛选行 ----------
    // “有效订单”列统计的是所选日期内未取消的订单数，因此需要日期筛选框。
    {
        auto* dr = new QHBoxLayout;                  // 日期行横向布局
        dr->addWidget(new QLabel(QStringLiteral("订单日期"))); // “订单日期”标签
        busDate_ = new QDateEdit(QDate::currentDate());        // 日期框，初值为今天
        busDate_->setCalendarPopup(true);            // 点击右侧箭头弹出日历
        busDate_->setMinimumWidth(160);              // 最小宽度，保证 yyyy/MM/dd 完整显示
        busDate_->setMaximumDate(QDate::currentDate()); // 上限为今天，不允许选未来日期
        dr->addWidget(busDate_);                     // 日期框加入日期行
        dr->addStretch();                            // 让控件靠左对齐
        bl->addLayout(dr);                           // 日期行加入班次页
        // 用户切换日期后，立即按新日期重新统计并刷新表格
        connect(busDate_, &QDateEdit::dateChanged, this, [this] { refreshBusTable(); });
        // 初始默认：数据中最新一条未取消订单的日期（没有订单时用今天）
        resetBusDateToLatest();
    }
    bl->addWidget(busTable_);                        // 表格加入页面

    // ---------- 班次操作按钮行 ----------
    auto* bb = new QHBoxLayout;                      // 按钮行横向布局
    auto* add = button("新增班次", true);            // “新增班次”主按钮
    auto* edit = button("修改时间");                 // “修改时间”按钮
    auto* dir = button("修改方向");                  // “修改方向”按钮
    auto* toggle = button("启用/停运");              // “启用/停运”按钮
    auto* del = button("删除班次");                  // “删除班次”按钮
    for (auto* b : {add, edit, dir, toggle, del}) bb->addWidget(b); // 依次加入按钮行
    bl->addLayout(bb);                               // 按钮行加入页面

    // 新增班次：依次输入班次编号、发车小时、方向，业务校验由 TransitStore::addBus 完成
    connect(add, &QPushButton::clicked, this, [this] {
        bool ok = false;                             // 对话框是否点了“确定”
        int id = QInputDialog::getInt(this, "新增班次", "班次编号", 7, 1, 999, 1, &ok); // 班次编号
        if (!ok) return;                             // 用户取消
        int hour = QInputDialog::getInt(this, "新增班次", "发车小时", 9, 0, 23, 1, &ok); // 发车小时
        if (!ok) return;
        QString dirText = QInputDialog::getItem(this, "新增班次", "方向",
                                                QStringList{QStringLiteral("S → E"), QStringLiteral("E → S")},
                                                0, false, &ok); // 行驶方向
        if (!ok) return;
        int d = dirText.startsWith("S") ? 0 : 1;     // 0 表示 S→E，1 表示 E→S
        std::string e;                               // 失败原因
        if (!store_.addBus(id, hour, d, e)) showError(errorText(e));
        else refreshBusTable();                      // 成功则刷新表格
    });

    // 修改发车时间：取选中班次的编号和当前小时数，再录入新的发车小时
    connect(edit, &QPushButton::clicked, this, [this] {
        int r = busTable_->currentRow();             // 当前选中的行号
        if (r < 0) { showError(QStringLiteral("请先选择要修改的班次。")); return; }
        int id = busTable_->item(r, 0)->text().toInt(); // 第 1 列是班次编号
        bool ok = false;
        int hour = QInputDialog::getInt(this, "修改发车时间", "发车小时",
                                        busTable_->item(r, 1)->text().left(2).toInt(), // 用“06:00”前两位作默认值
                                        0, 23, 1, &ok);
        if (!ok) return;
        std::string e;                               // 失败原因
        if (!store_.updateBus(id, hour, e)) showError(errorText(e));
        else refreshBusTable();
    });

    // 修改方向：存在当天有效订单时 TransitStore 会拒绝修改
    connect(dir, &QPushButton::clicked, this, [this] {
        int r = busTable_->currentRow();             // 当前选中的行号
        if (r < 0) { showError(QStringLiteral("请先选择要修改方向的班次。")); return; }
        int id = busTable_->item(r, 0)->text().toInt(); // 班次编号
        bool ok = false;
        QString dirText = QInputDialog::getItem(this, "修改方向", "方向",
                                                QStringList{QStringLiteral("S → E"), QStringLiteral("E → S")},
                                                0, false, &ok);
        if (!ok) return;
        int d = dirText.startsWith("S") ? 0 : 1;     // 0: S→E，1: E→S
        std::string e;
        if (!store_.updateBusDirection(id, d, e)) showError(errorText(e));
        else refreshBusTable();
    });

    // 启用/停运切换：当天有有效订单的班次不允许停运
    connect(toggle, &QPushButton::clicked, this, [this] {
        int r = busTable_->currentRow();             // 当前选中的行号
        if (r < 0) { showError(QStringLiteral("请先选择要启用或停运的班次。")); return; }
        std::string e;
        if (!store_.toggleBus(busTable_->item(r, 0)->text().toInt(), e)) showError(errorText(e));
        else refreshBusTable();
    });

    // 删除班次：先二次确认；有历史订单记录的班次不允许删除
    connect(del, &QPushButton::clicked, this, [this] {
        int r = busTable_->currentRow();             // 当前选中的行号
        if (r < 0) { showError(QStringLiteral("请先选择要删除的班次。")); return; }
        if (QMessageBox::question(this, "删除班次", "确认删除当前班次？") != QMessageBox::Yes) return;
        std::string e;
        if (!store_.deleteBus(busTable_->item(r, 0)->text().toInt(), e)) showError(errorText(e));
        else refreshBusTable();
    });
    tabs->addTab(buses, "班次管理");                 // 班次页加入标签页
    // ==================== 标签页 2：订单管理 ====================
    auto* orders = new QWidget;                      // 订单管理页容器
    auto* rl = new QVBoxLayout(orders);              // 纵向布局：筛选行 → 订单表格
    auto* filterRow = new QHBoxLayout;               // 筛选行横向布局
    orderFilter_ = new QLineEdit;                    // 订单筛选输入框
    orderFilter_->setPlaceholderText("按订单号、手机号或班次筛选"); // 输入提示
    filterRow->addWidget(orderFilter_);              // 输入框靠左
    auto* cancelOrder = button("取消订单");          // 管理员取消订单按钮
    filterRow->addWidget(cancelOrder);               // 取消按钮靠右
    rl->addLayout(filterRow);                        // 筛选行加入页面
    orderTable_ = new QTableWidget;                  // 订单表格（只读）
    orderTable_->setColumnCount(9);                  // 9 列
    orderTable_->setHorizontalHeaderLabels({"订单号", "日期", "班次", "乘客", "手机号",
                                            "路线", "座位", "票价", "状态"}); // 表头
    orderTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);          // 只读，禁止直接改单元格
    orderTable_->horizontalHeader()->setStretchLastSection(true);             // 最后一列铺满
    rl->addWidget(orderTable_);                      // 表格加入页面
    // 筛选框内容变化时实时过滤订单列表
    connect(orderFilter_, &QLineEdit::textChanged, this, [this] { refreshOrderTable(); });
    // 取消选中订单：管理员可越过“只能操作自己的订单”的限制；
    // 取消成功后同时刷新订单表和班次表（有效订单数可能变化）
    connect(cancelOrder, &QPushButton::clicked, this, [this] {
        int r = orderTable_->currentRow();           // 当前选中的行号
        if (r < 0) return;
        std::string e;                               // 失败原因
        if (!store_.cancelOrder(orderTable_->item(r, 0)->text().toStdString(), // 第 1 列是订单号
                                 {}, true, e))       // phone 留空，true 表示管理员权限
            showError(errorText(e));
        else { refreshOrderTable(); refreshBusTable(); }
    });
    tabs->addTab(orders, "订单管理");                 // 订单页加入标签页
    // ==================== 标签页 3：乘客管理 ====================
    auto* accounts = new QWidget;                    // 乘客管理页容器
    auto* al = new QVBoxLayout(accounts);            // 纵向布局：表格 → 按钮行
    accountTable_ = new QTableWidget;                // 乘客表格（只读）
    accountTable_->setColumnCount(3);                // 3 列：手机号、姓名、状态
    accountTable_->setHorizontalHeaderLabels({"手机号", "姓名", "状态"}); // 表头
    accountTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);    // 只读
    accountTable_->horizontalHeader()->setStretchLastSection(true);       // 最后一列铺满
    al->addWidget(accountTable_);                    // 表格加入页面
    auto* ab = new QHBoxLayout;                      // 按钮行横向布局
    auto* aa = button("新增乘客", true);             // “新增乘客”主按钮
    auto* ap = button("重置密码");                   // “重置密码”按钮
    auto* at = button("启用/停用");                  // “启用/停用”按钮
    auto* ad = button("删除账户");                   // “删除账户”按钮
    for (auto* b : {aa, ap, at, ad}) ab->addWidget(b); // 依次加入按钮行
    al->addLayout(ab);                               // 按钮行加入页面

    // 新增乘客：依次录入手机号、姓名、密码，校验由 TransitStore::addAccount 完成
    connect(aa, &QPushButton::clicked, this, [this] {
        bool ok = false;                             // 对话框是否点了“确定”
        QString phone = QInputDialog::getText(this, "新增乘客", "手机号",
                                              QLineEdit::Normal, {}, &ok);
        if (!ok) return;
        QString name = QInputDialog::getText(this, "新增乘客", "姓名",
                                             QLineEdit::Normal, {}, &ok);
        if (!ok) return;
        QString pass = QInputDialog::getText(this, "新增乘客", "密码",
                                             QLineEdit::Password, {}, &ok); // 密码框掩码显示
        if (!ok) return;
        std::string e;                               // 失败原因
        // 组装 Account：手机号、密码、姓名、启用状态（新账户默认启用）
        if (!store_.addAccount({phone.toStdString(), pass.toStdString(),
                                name.toUtf8().toStdString(), true}, e))
            showError(errorText(e));
        else refreshAccountTable();
    });

    // 重置密码：把选中乘客的密码改为新录入的密码
    connect(ap, &QPushButton::clicked, this, [this] {
        int r = accountTable_->currentRow();         // 当前选中的行号
        if (r < 0) return;
        bool ok = false;
        QString pass = QInputDialog::getText(this, "重置密码", "新密码",
                                             QLineEdit::Password, {}, &ok);
        if (!ok) return;
        std::string e;
        if (!store_.resetPassword(accountTable_->item(r, 0)->text().toStdString(), // 手机号
                                  pass.toStdString(), e))
            showError(errorText(e));
        else showInfo("密码已更新");
    });

    // 启用/停用账号：切换选中乘客的 enabled 状态
    connect(at, &QPushButton::clicked, this, [this] {
        int r = accountTable_->currentRow();         // 当前选中的行号
        if (r < 0) return;
        std::string e;
        if (!store_.toggleAccount(accountTable_->item(r, 0)->text().toStdString(), e))
            showError(errorText(e));
        else refreshAccountTable();
    });

    // 删除账户：存在当天有效订单的乘客不允许删除
    connect(ad, &QPushButton::clicked, this, [this] {
        int r = accountTable_->currentRow();         // 当前选中的行号
        if (r < 0) return;
        std::string e;
        if (!store_.deleteAccount(accountTable_->item(r, 0)->text().toStdString(), e))
            showError(errorText(e));
        else refreshAccountTable();
    });
    tabs->addTab(accounts, "乘客管理");               // 乘客页加入标签页
    // ==================== 标签页 4：营收统计 ====================
    auto* revenue = new QWidget;                     // 营收统计页容器
    auto* vl = new QVBoxLayout(revenue);             // 纵向布局：日期/按钮行 → 汇总文本
    auto* vr = new QHBoxLayout;                      // 日期与按钮横向布局
    revenueDate_ = new QDateEdit(QDate::currentDate()); // 统计日期框，初值今天
    revenueDate_->setCalendarPopup(true);            // 点击弹出日历
    revenueDate_->setMinimumWidth(160);              // 保证日期完整显示
    {
        // 日期上限 = 最新一条未取消订单的日期（没有订单时取今天），
        // 默认也定位到该日期，避免打开页面就统计到一个没有订单的日期。
        const QDate latest = latestActiveOrderDate(); // 最新有效订单日期
        revenueDate_->setMaximumDate(latest.isValid() ? latest : QDate::currentDate());
        if (latest.isValid()) revenueDate_->setDate(latest);
    }
    auto* query = button("统计所选日期", true);      // “统计所选日期”主按钮
    auto* all = button("统计全部");                  // “统计全部”按钮
    vr->addWidget(revenueDate_);                     // 日期框
    vr->addWidget(query);                            // 统计所选日期按钮
    vr->addWidget(all);                              // 统计全部按钮
    vl->addLayout(vr);                               // 操作行加入页面
    revenueSummary_ = new QLabel;                    // 汇总文本标签
    vl->addWidget(revenueSummary_);                  // 加入页面
    vl->addStretch();                                // 底部留白
    // 统计所选日期：把“all”属性置 false，refreshRevenue 会按日期过滤
    connect(query, &QPushButton::clicked, this, [this] {
        revenueDate_->setProperty("all", false);
        refreshRevenue();
    });
    // 统计全部：把“all”属性置 true，refreshRevenue 忽略日期统计所有未取消订单
    connect(all, &QPushButton::clicked, this, [this] {
        revenueDate_->setProperty("all", true);
        refreshRevenue();
    });
    tabs->addTab(revenue, "营收统计");                // 营收页加入标签页
    // ==================== 标签页 5：数据维护 ====================
    auto* maintenance = new QWidget;                 // 数据维护页容器
    auto* ml = new QVBoxLayout(maintenance);         // 纵向布局
    ml->addWidget(new QLabel("文本数据保存于程序目录 data 文件夹，可进行备份和恢复。")); // 说明文字
    auto* backup = button("备份全部数据", true);     // “备份全部数据”主按钮
    auto* restore = button("恢复数据");              // “恢复数据”按钮
    ml->addWidget(backup);                           // 备份按钮加入页面
    ml->addWidget(restore);                          // 恢复按钮加入页面
    ml->addStretch();                                // 底部留白
    // 备份：选择目标目录，把 bus/passenger/admin/orders 四个数据文件复制过去
    connect(backup, &QPushButton::clicked, this, [this] {
        auto dir = QFileDialog::getExistingDirectory(this, "选择备份目录"); // 备份目录
        if (dir.isEmpty()) return;                   // 用户取消
        std::string e;                               // 失败原因
        if (!store_.backup(dir.toStdString(), e)) showError(errorText(e));
        else showInfo("备份完成");
    });
    // 恢复：选择备份目录，二次确认后覆盖当前数据并重新加载
    connect(restore, &QPushButton::clicked, this, [this] {
        auto dir = QFileDialog::getExistingDirectory(this, "选择备份目录"); // 备份目录
        if (dir.isEmpty()) return;
        if (QMessageBox::question(this, "恢复数据", "恢复会覆盖当前数据，确认继续？") != QMessageBox::Yes) return;
        std::string e;
        if (!store_.restore(dir.toStdString(), e)) showError(errorText(e));
        else {
            // 数据已整体替换，三个表格都要按新数据重新填充
            refreshBusTable();
            refreshOrderTable();
            refreshAccountTable();
            showInfo("恢复完成");
        }
    });
    tabs->addTab(maintenance, "数据维护");            // 数据维护页加入标签页
    pages_->addWidget(adminPage_);                   // 管理员工作台加入页面栈
}

int MainWindow::currentFrom() const { return fromStop_ ? fromStop_->currentData().toInt() : 0; }
int MainWindow::currentTo() const { return toStop_ ? toStop_->currentData().toInt() : 3; }

int MainWindow::effectiveNowMinutes() const {
    if (demoTime_ && demoTime_->isChecked() && demoTimeEdit_) {
        return demoTimeEdit_->time().hour() * 60 +
               demoTimeEdit_->time().minute();
    }
    return transit::systemNowMinutes();
}

void MainWindow::refreshBusList(bool preserveSelection) {
    if (!busList_) return;
    const int keepBus = preserveSelection ? selectedBusId_ : 0;
    const int keepRow = preserveSelection ? selectedRow_ : -1;
    const int keepCol = preserveSelection ? selectedCol_ : -1;
    busList_->clear();
    selectedBusId_ = 0;
    selectedRow_ = selectedCol_ = -1;

    const int direction = direction_->currentIndex();
    const int from = currentFrom();
    const int to = currentTo();
    if (transit::stopPosition(direction, from) >= transit::stopPosition(direction, to)) {
        querySummary_->setText(QStringLiteral("请选择正确的起点站和到达站"));
        refreshSeatMap();
        return;
    }

    const int now = effectiveNowMinutes();
    for (const auto& bus : store_.buses()) {
        if (!bus.enabled || bus.direction != direction) continue;
        const int departure = bus.hour * 60;
        const int finish = departure + transit::kSegments * transit::kSegmentMinutes;
        const int boarding = store_.arrivalMinutes(bus, from);
        int nextPosition = -1;
        int remain = 0;
        for (int position = 0; position < transit::kStops; ++position) {
            const int arrival = departure + position * transit::kSegmentMinutes;
            if (arrival > now) {
                nextPosition = position;
                remain = arrival - now;
                break;
            }
        }
        QString status;
        if (now >= finish) {
            status = QStringLiteral("当日已结束");
        } else if (now >= boarding) {
            status = QStringLiteral("已驶过起点站 · 不可预约");
        } else if (now >= departure) {
            if (nextPosition >= 0) {
                status = QStringLiteral("已发车 · 尚未到起点站（下一站 %1 · 还有 %2）")
                             .arg(text(transit::stopName(transit::stopAt(direction, nextPosition))))
                             .arg(QString::fromStdString(transit::formatDuration(remain)));
            } else {
                status = QStringLiteral("已发车 · 尚未到起点站");
            }
        } else {
            status = QStringLiteral("下一站 %1 · 还有 %2")
                         .arg(text(transit::stopName(transit::stopAt(direction, nextPosition))))
                         .arg(QString::fromStdString(transit::formatDuration(remain)));
        }
        const bool canBook = now < boarding;
        const QString availability = canBook
            ? QStringLiteral("可预约座位 %1 / 15").arg(store_.availableSeats(bus, from, to))
            : QStringLiteral("暂不可预约 · 当前路线可用座位 %1 / 15").arg(store_.availableSeats(bus, from, to));
        const QString label = QStringLiteral("班次 %1 · %2 · %3\n%4 · %5")
                                  .arg(bus.id)
                                  .arg(QString::fromStdString(transit::formatClock(departure)))
                                  .arg(text(transit::directionName(direction)))
                                  .arg(status)
                                  .arg(availability);
        auto* item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, bus.id);
        const int itemWidth = busList_->viewport()->width() > 0
                                  ? busList_->viewport()->width() - 24
                                  : 300;
        item->setSizeHint(QSize(itemWidth, 58));
        busList_->addItem(item);
    }
    if (keepBus != 0) {
        for (int row = 0; row < busList_->count(); ++row) {
            if (busList_->item(row)->data(Qt::UserRole).toInt() == keepBus) {
                selectedBusId_ = keepBus;
                selectedRow_ = keepRow;
                selectedCol_ = keepCol;
                QSignalBlocker blocker(busList_);
                busList_->setCurrentRow(row);
                break;
            }
        }
    }
    querySummary_->setText(QStringLiteral("系统时间：%1 · 计算时间：%2 · 请选择一个班次")
                               .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"))
                               .arg(QString::fromStdString(transit::formatClock(effectiveNowMinutes()))));
    refreshSeatMap();
}

void MainWindow::refreshSeatMap() {
    if (!seatPanel_) return;
    clearPanel(seatPanel_);
    auto* grid = new QGridLayout(seatPanel_);
    grid->setSpacing(8);
    if (!selectedBusId_) {
        grid->addWidget(new QLabel(QStringLiteral("请选择班次查看 5×4 座位图")), 0, 0);
        bookButton_->setEnabled(false);
        return;
    }
    const auto it = std::find_if(store_.buses().begin(), store_.buses().end(),
                                 [&](const transit::Bus& bus) { return bus.id == selectedBusId_; });
    if (it == store_.buses().end()) {
        bookButton_->setEnabled(false);
        return;
    }
    const auto& bus = *it;
    const int now = effectiveNowMinutes();
    const int departure = bus.hour * 60;
    const int boarding = store_.arrivalMinutes(bus, currentFrom());
    const bool canBook = now < boarding;
    QString bookingHint;
    if (now < departure) {
        bookingHint = QStringLiteral("发车前可预约");
    } else if (canBook) {
        bookingHint = QStringLiteral("已发车，尚未到起点站，可预约");
    } else {
        bookingHint = QStringLiteral("已驶过起点站，不可预约");
    }
    querySummary_->setText(QStringLiteral("班次 %1 · %2 → %3 · 票价 %4 元 · 可用座位 %5 · %6")
                               .arg(bus.id)
                               .arg(text(transit::stopName(currentFrom())))
                               .arg(text(transit::stopName(currentTo())))
                               .arg(transit::priceByStops(transit::stopPosition(bus.direction, currentTo()) -
                                                          transit::stopPosition(bus.direction, currentFrom())))
                               .arg(store_.availableSeats(bus, currentFrom(), currentTo()))
                               .arg(bookingHint));
    grid->addWidget(new QLabel(canBook ? QStringLiteral("座位图 · 请选择一个空座位")
                                       : QStringLiteral("座位图 · 班次已驶过起点站，当前不可预约")), 0, 0, 1, 4);
    for (int col = 0; col < transit::kCols; ++col)
        grid->addWidget(new QLabel(QStringLiteral("%1 列").arg(col + 1)), 1, col + 1);
    for (int row = 0; row < transit::kRows; ++row) {
        grid->addWidget(new QLabel(QStringLiteral("第 %1 排").arg(row + 1)), row + 2, 0);
        for (int col = 0; col < transit::kCols; ++col) {
            auto* seat = new QPushButton(QStringLiteral("%1-%2").arg(row + 1).arg(col + 1));
            const int status = store_.seatStatus(bus, currentFrom(), currentTo(), row, col);
            if (status == transit::SEAT_NONE) {
                seat->setEnabled(false);
                seat->setText(QStringLiteral("无座"));
                seat->setStyleSheet("background:#e4e9ed;color:#9aa8af;");
            } else if (status == transit::SEAT_BOOKED) {
                seat->setEnabled(false);
                seat->setText(QStringLiteral("已订"));
                seat->setStyleSheet("background:#ffd7b0;color:#9a5b13;");
            } else if (!canBook) {
                seat->setEnabled(false);
                seat->setText(QStringLiteral("不可约"));
                seat->setStyleSheet("background:#e4e9ed;color:#9aa8af;");
            } else if (selectedRow_ == row && selectedCol_ == col) {
                seat->setStyleSheet("background:#2f80ed;color:white;");
            } else {
                seat->setStyleSheet("background:#d5f3ef;color:#087e7e;");
                connect(seat, &QPushButton::clicked, this, [this, row, col] {
                    selectedRow_ = row;
                    selectedCol_ = col;
                    refreshSeatMap();
                });
            }
            grid->addWidget(seat, row + 2, col + 1);
        }
    }
    bookButton_->setEnabled(canBook && selectedRow_ >= 0);
}

void MainWindow::refreshClockAndAvailability() {
    const QDateTime now = QDateTime::currentDateTime();
    QString clockText = QStringLiteral("系统时间：%1")
                            .arg(now.toString("yyyy-MM-dd HH:mm:ss"));
    if (demoTime_ && demoTime_->isChecked() && demoTimeEdit_) {
        clockText += QStringLiteral(" · 演示时间：%1")
                         .arg(demoTimeEdit_->time().toString("HH:mm"));
    }
    if (clockLabel_) clockLabel_->setText(clockText);

    const QString today = QString::fromStdString(transit::systemDateText());
    if (today != activeDate_) {
        std::string error;
        if (!store_.refreshForToday(error)) showError(errorText(error));
        activeDate_ = today;
        lastNowMinutes_ = -1;
        if (!currentPhone_.isEmpty()) {
            refreshPassengerOrders();
            refreshBusList(true);
        }
    }

    // 勾选“使用演示时间”时，分钟数由用户指定，不再跟着系统分钟自动刷新；
    // 但日期仍以系统日期为准，日期变化时上面已经强制刷新座位。
    const int nowMinutes = effectiveNowMinutes();
    if (nowMinutes != lastNowMinutes_) {
        lastNowMinutes_ = nowMinutes;
        if (!currentPhone_.isEmpty()) refreshBusList(true);
    }
}

void MainWindow::refreshPassengerOrders() {
    if (!passengerOrders_) return;
    passengerOrders_->setRowCount(0);
    const std::string today = transit::systemDateText();
    for (const auto& ticket : store_.orders()) {
        if (ticket.phone != currentPhone_.toStdString()) continue;
        const int row = passengerOrders_->rowCount();
        passengerOrders_->insertRow(row);
        passengerOrders_->setItem(row, 0, new QTableWidgetItem(text(ticket.id)));
        passengerOrders_->setItem(row, 1, new QTableWidgetItem(text(ticket.date)));
        passengerOrders_->setItem(row, 2, new QTableWidgetItem(QString::number(ticket.busId)));
        passengerOrders_->setItem(row, 3, new QTableWidgetItem(QStringLiteral("%1 → %2")
            .arg(text(transit::stopName(ticket.from))).arg(text(transit::stopName(ticket.to)))));
        passengerOrders_->setItem(row, 4, new QTableWidgetItem(QStringLiteral("%1-%2").arg(ticket.row + 1).arg(ticket.col + 1)));
        passengerOrders_->setItem(row, 5, new QTableWidgetItem(QString::number(ticket.price) + QStringLiteral(" 元")));
        const bool todayActive = ticket.status == transit::ORDER_ACTIVE && ticket.date == today;
        const QString status = ticket.status == transit::ORDER_CANCELLED
            ? QStringLiteral("已取消") : (todayActive ? QStringLiteral("有效") : QStringLiteral("已结束"));
        passengerOrders_->setItem(row, 6, new QTableWidgetItem(status));
        passengerOrders_->setItem(row, 7, new QTableWidgetItem(todayActive ? QStringLiteral("取消") : QStringLiteral("—")));
    }
}
/*
 * 刷新班次管理表格。
 * 功能：按 busDate_ 所选日期，为每个班次填入编号、发车时间、方向、状态、
 *       当日未取消订单数和今天全程可用座位数。
 * 参数：无。
 * 返回值：无。
 */
void MainWindow::refreshBusTable() {
    if (!busTable_) return;                          // 页面尚未构建，直接返回
    // 所选日期，格式 yyyy-MM-dd，用于统计第 5 列
    const QString dateText = busDate_ ? busDate_->date().toString("yyyy-MM-dd") : QString();
    busTable_->setRowCount(0);                       // 先清空旧数据
    for (const auto& b : store_.buses()) {           // b：当前遍历到的班次
        const int r = busTable_->rowCount();         // 新行号（当前行数）
        busTable_->insertRow(r);                     // 插入一行
        // 第 1 列：班次编号
        busTable_->setItem(r, 0, new QTableWidgetItem(QString::number(b.id)));
        // 第 2 列：发车时间（由发车小时换算成 HH:mm）
        busTable_->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(transit::formatClock(b.hour * 60))));
        // 第 3 列：方向名称（S → E 或 E → S）
        busTable_->setItem(r, 2, new QTableWidgetItem(text(transit::directionName(b.direction))));
        // 第 4 列：运行状态
        busTable_->setItem(r, 3, new QTableWidgetItem(b.enabled ? "运行" : "停运"));
        // 第 5 列：所选日期内该班次未取消的订单数
        busTable_->setItem(r, 4, new QTableWidgetItem(
            QString::number(store_.ticketsForBusOnDate(b.id, dateText.toStdString()))));
        // 第 6 列：今天全程仍空余的座位数（由座位占用情况计算，不可手工修改）
        busTable_->setItem(r, 5, new QTableWidgetItem(QString::number(store_.wholeRouteFree(b))));
    }
}

/*
 * 把“订单日期”重置为数据中最新一条未取消订单的日期。
 * 功能：没有订单或最新日期晚于今天时回退到今天；同时把日期上限设为今天。
 * 参数：无。
 * 返回值：无。
 */
void MainWindow::resetBusDateToLatest() {
    if (!busDate_) return;                           // 控件尚未创建，直接返回
    busDate_->setMaximumDate(QDate::currentDate());  // 不允许选择未来日期
    const QDate latest = latestActiveOrderDate();    // 最新未取消订单日期
    // 设置日期时屏蔽 dateChanged 信号，避免刷新两次；最后由本函数统一刷新表格
    busDate_->blockSignals(true);
    busDate_->setDate((latest.isValid() && latest <= QDate::currentDate())
                          ? latest : QDate::currentDate()); // 有订单取最新日期，否则今天
    busDate_->blockSignals(false);
    refreshBusTable();                               // 按最终日期刷新表格
}
/*
 * 刷新订单管理表格。
 * 功能：按 orderFilter_ 的关键字过滤订单，然后逐行显示订单号、日期、班次、乘客、
 *       手机号、路线、座位、票价和状态。
 * 状态规则：已取消；有效（今天且未取消）；已结束（历史日期的未取消订单）。
 * 参数：无。
 * 返回值：无。
 */
void MainWindow::refreshOrderTable() {
    if (!orderTable_) return;                        // 页面尚未构建
    orderTable_->setRowCount(0);                     // 清空旧数据
    // 筛选关键字，去掉首尾空格；没有输入框时为空
    const QString filter = orderFilter_ ? orderFilter_->text().trimmed() : QString();
    const std::string today = transit::systemDateText(); // 系统今天的日期
    for (const auto& ticket : store_.orders()) {     // ticket：当前遍历到的订单
        // 用于匹配的文本：订单号 + 手机号 + 班次号
        const QString line = text(ticket.id) + text(ticket.phone) + QString::number(ticket.busId);
        // 有关键字且匹配不上时跳过该订单（忽略大小写）
        if (!filter.isEmpty() && !line.contains(filter, Qt::CaseInsensitive)) continue;
        const int row = orderTable_->rowCount();     // 新行号
        orderTable_->insertRow(row);                 // 插入一行
        // 是否为“今天且未取消”的有效订单
        const bool todayActive = ticket.status == transit::ORDER_ACTIVE && ticket.date == today;
        // 状态文本：已取消优先，其次区分今天的有效订单和历史订单
        const QString status = ticket.status == transit::ORDER_CANCELLED
            ? QStringLiteral("已取消") : (todayActive ? QStringLiteral("有效") : QStringLiteral("已结束"));
        // 一行 9 列的数据，顺序与表头一致
        const QStringList values = {
            text(ticket.id),                                                 // 订单号
            text(ticket.date),                                               // 下单日期
            QString::number(ticket.busId),                                   // 班次编号
            text(ticket.name),                                               // 乘客姓名
            text(ticket.phone),                                              // 乘客手机号
            QStringLiteral("%1 → %2").arg(text(transit::stopName(ticket.from)))
                                     .arg(text(transit::stopName(ticket.to))), // 起点 → 终点
            QStringLiteral("%1-%2").arg(ticket.row + 1).arg(ticket.col + 1), // 座位号（行列从 1 开始）
            QString::number(ticket.price),                                   // 票价
            status                                                           // 状态
        };
        // 依次写入每一列
        for (int col = 0; col < values.size(); ++col)
            orderTable_->setItem(row, col, new QTableWidgetItem(values[col]));
    }
}
/*
 * 刷新乘客管理表格。
 * 功能：显示所有乘客账号的手机号、姓名和启用/停用状态。
 * 参数：无。
 * 返回值：无。
 */
void MainWindow::refreshAccountTable() {
    if (!accountTable_) return;                      // 页面尚未构建
    accountTable_->setRowCount(0);                   // 清空旧数据
    for (const auto& a : store_.accounts()) {        // a：当前遍历到的乘客账号
        const int r = accountTable_->rowCount();     // 新行号
        accountTable_->insertRow(r);                 // 插入一行
        accountTable_->setItem(r, 0, new QTableWidgetItem(text(a.phone)));   // 第 1 列：手机号
        accountTable_->setItem(r, 1, new QTableWidgetItem(text(a.name)));    // 第 2 列：姓名
        accountTable_->setItem(r, 2, new QTableWidgetItem(a.enabled ? "启用" : "停用")); // 第 3 列：状态
    }
}

/*
 * 查找数据中最新一条“未取消”订单的日期。
 * 功能：遍历全部订单，忽略已取消订单，取日期最大值。
 * 参数：无。
 * 返回值：最新未取消订单日期；若没有未取消订单，返回无效 QDate。
 */
QDate MainWindow::latestActiveOrderDate() const {
    QDate latest;                                    // 当前找到的最新日期，初始无效
    for (const auto& t : store_.orders()) {          // t：当前遍历到的订单
        if (t.status != transit::ORDER_ACTIVE) continue; // 跳过已取消订单
        const QDate d = QDate::fromString(text(t.date), "yyyy-MM-dd"); // 解析订单日期
        // 日期有效且比当前记录更新时更新 latest
        if (d.isValid() && (!latest.isValid() || d > latest)) latest = d;
    }
    return latest;                                   // 无未取消订单时返回无效 QDate
}

/*
 * 刷新营收统计。
 * 功能：按“统计全部”或所选日期统计未取消订单的订单数和营业额，
 *       并按班次、票价分类汇总；所选日期没有订单时给出提示。
 * 参数：无（统计方式由 revenueDate_ 的 all 属性和日期决定）。
 * 返回值：无（结果写入 revenueSummary_）。
 */
void MainWindow::refreshRevenue() {
    if (!revenueSummary_) return;                    // 页面尚未构建
    {
        // 日期上限：不允许选择最新未取消订单日期之后的日期；
        // 一条订单都没有时上限取今天。
        const QDate latest = latestActiveOrderDate(); // 最新未取消订单日期
        revenueDate_->setMaximumDate(latest.isValid() ? latest : QDate::currentDate());
    }
    // “all”属性由“统计全部”按钮置 true、“统计所选日期”按钮置 false
    const bool all = revenueDate_->property("all").toBool();
    // 当前选择的日期，格式 yyyy-MM-dd
    const QString dateText = revenueDate_->date().toString("yyyy-MM-dd");
    // 统计结果：all 为 true 时传空日期表示统计全部，否则只统计所选日期
    const auto s = store_.revenue(all ? std::string() : dateText.toStdString());
    // 把 map<编号, 数值> 拼成“标签：数值”形式的文本，例如“班次 2：100 元”
    auto formatMap = [&](const std::map<int, int>& m, const QString& unit) {
        QString v;                                   // 拼接结果，初始为空
        for (auto it = m.begin(); it != m.end(); ++it)
            v += unit.arg(it->first).arg(it->second); // unit 中的 %1 为编号、%2 为数值
        return v.isEmpty() ? QStringLiteral("无") : v; // 没有任何数据时显示“无”
    };
    QString hint;                                    // 追加在汇总后面的提示文字
    if (!all && s.activeCount == 0) {                // 所选日期没有未取消订单
        QStringList dates;                           // 有未取消订单的日期集合
        for (const auto& t : store_.orders()) {      // t：当前遍历到的订单
            if (t.status == transit::ORDER_ACTIVE) { // 只看未取消订单
                const QString d = text(t.date);      // 订单日期
                if (!dates.contains(d)) dates.append(d); // 去重后加入
            }
        }
        std::sort(dates.begin(), dates.end());       // 日期升序，便于展示
        hint = dates.isEmpty()                       // 整个系统都没有订单
            ? QStringLiteral("\n\n提示：系统中还没有任何未取消订单。")
            : QStringLiteral("\n\n提示：%1 没有未取消订单。有未取消订单的日期：%2\n可改选上述日期，或点击“统计全部”。")
                  .arg(dateText).arg(dates.join(QStringLiteral("、")));
    }
    // 组装汇总文字：统计范围、订单数、营业额、按班次、按票价，最后追加提示
    revenueSummary_->setText(QStringLiteral("统计范围：%1\n未取消订单：%2 张\n营业额：%3 元\n按班次：%4\n按票价：%5%6")
        .arg(all ? QStringLiteral("全部数据") : dateText) // 统计范围
        .arg(s.activeCount)                              // 未取消订单数
        .arg(s.total)                                    // 营业额合计
        .arg(formatMap(s.byBus, QStringLiteral("班次 %1：%2 元  ")))   // 按班次分布
        .arg(formatMap(s.byPrice, QStringLiteral("%1 元：%2 张  ")))   // 按票价分布
        .arg(hint));                                     // 无数据时的提示
}

void MainWindow::showError(const QString& message){QMessageBox::warning(this,QStringLiteral("操作提示"),message);}
void MainWindow::showInfo(const QString& message){QMessageBox::information(this,QStringLiteral("操作成功"),message);}
void MainWindow::leaveSession(){currentPhone_.clear();currentAdmin_.clear();pages_->setCurrentWidget(welcomePage_);}
