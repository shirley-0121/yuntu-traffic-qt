#pragma once

#include "transit_core.h"

#include <QMainWindow>
#include <QDate>

class QComboBox;
class QLabel;
class QListWidget;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTabWidget;
class QDateEdit;
class QTimer;
class QCheckBox;
class QTimeEdit;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    transit::TransitStore store_;
    QStackedWidget* pages_ = nullptr;
    QString currentPhone_;
    QString currentAdmin_;

    QWidget* welcomePage_ = nullptr;
    QWidget* passengerLoginPage_ = nullptr;
    QWidget* registerPage_ = nullptr;
    QWidget* adminLoginPage_ = nullptr;
    QWidget* passengerPage_ = nullptr;
    QWidget* adminPage_ = nullptr;   // 管理员工作台整页容器（含顶部栏与 5 个标签页）

    QLineEdit* passengerPhone_ = nullptr;
    QLineEdit* passengerPassword_ = nullptr;
    QLineEdit* registerPhone_ = nullptr;
    QLineEdit* registerPassword_ = nullptr;
    QLineEdit* registerName_ = nullptr;
    QLineEdit* adminAccount_ = nullptr;
    QLineEdit* adminPassword_ = nullptr;

    QComboBox* direction_ = nullptr;
    QComboBox* fromStop_ = nullptr;
    QComboBox* toStop_ = nullptr;
    QCheckBox* demoTime_ = nullptr;
    QTimeEdit* demoTimeEdit_ = nullptr;
    QTimer* clockTimer_ = nullptr;
    QLabel* clockLabel_ = nullptr;
    QString activeDate_;
    int lastNowMinutes_ = -1;
    QListWidget* busList_ = nullptr;
    QWidget* seatPanel_ = nullptr;
    QLabel* querySummary_ = nullptr;
    QPushButton* bookButton_ = nullptr;
    QTableWidget* passengerOrders_ = nullptr;
    int selectedBusId_ = 0;
    int selectedRow_ = -1;
    int selectedCol_ = -1;

    // ---------------- 管理员工作台控件 ----------------
    QTableWidget* busTable_ = nullptr;      // 班次管理表格：班次号、发车时间、方向、状态、有效订单、区间可用座位
    QDateEdit* busDate_ = nullptr;          // 班次管理“订单日期”选择框：决定“有效订单”列统计哪一天
    QTableWidget* orderTable_ = nullptr;    // 订单管理表格：管理员查看/筛选全部订单，只读
    QTableWidget* accountTable_ = nullptr;  // 乘客管理表格：手机号、姓名、启用状态，只读
    QLineEdit* orderFilter_ = nullptr;      // 订单筛选输入框：按订单号、手机号或班次号过滤
    QDateEdit* revenueDate_ = nullptr;      // 营收统计日期选择框：可选择有订单的日期
    QLabel* revenueSummary_ = nullptr;      // 营收统计汇总文本：订单数、营业额、按班次/票价分布

    void buildPages();
    void buildWelcome();
    void buildPassengerLogin();
    void buildRegister();
    void buildAdminLogin();
    void buildPassengerPage();
    /*
     * 构建管理员工作台页面（不包含管理员登录页）。
     * 功能：顶部栏 + 班次管理/订单管理/乘客管理/营收统计/数据维护五个标签页，
     *       并绑定各按钮的点击事件。
     * 参数：无。
     * 返回值：无（页面保存到成员 adminPage_）。
     */
    void buildAdminPage();

    void refreshBusList(bool preserveSelection = false);
    void refreshSeatMap();
    void refreshPassengerOrders();
    /*
     * 按 busDate_ 所选日期重新填充班次管理表格。
     * 参数：无（日期来自 busDate_）。
     * 返回值：无。
     */
    void refreshBusTable();
    /*
     * 把“订单日期”重置为数据中最新一条未取消订单的日期；没有订单时用今天，
     * 并把日期上限设为今天（不允许选择未来日期）。
     * 参数：无。
     * 返回值：无。
     */
    void resetBusDateToLatest();
    /*
     * 刷新订单管理表格：按 orderFilter_ 的内容过滤后显示全部订单及其状态。
     * 参数：无（筛选关键字来自 orderFilter_）。
     * 返回值：无。
     */
    void refreshOrderTable();
    /*
     * 刷新乘客管理表格：显示所有乘客账号的手机号、姓名、启用/停用状态。
     * 参数：无。
     * 返回值：无。
     */
    void refreshAccountTable();
    /*
     * 刷新营收统计：按 revenueDate_ 所选日期或“统计全部”生成营业额汇总，
     * 并在所选日期无未取消订单时提示哪些日期有数据。
     * 参数：无。
     * 返回值：无。
     */
    void refreshRevenue();
    void refreshClockAndAvailability();
    void showError(const QString& message);
    void showInfo(const QString& message);
    QString text(const std::string& value) const;
    QString text(const char* value) const;
    int currentFrom() const;
    int currentTo() const;
    int effectiveNowMinutes() const;
    /*
     * 查找数据中最新一条“未取消”订单的日期。
     * 参数：无。
     * 返回值：最新订单日期；若没有任何未取消订单，返回无效 QDate（isValid() 为 false）。
     */
    QDate latestActiveOrderDate() const;
    void leaveSession();
};
