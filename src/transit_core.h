#pragma once

#include <array>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace transit {

constexpr int kStops = 4;              // 线路站点数：S、A、B、E
constexpr int kSegments = 3;           // 线路区间数：S-A、A-B、B-E
constexpr int kRows = 5;               // 座位图行数
constexpr int kCols = 4;               // 座位图列数
constexpr int kSegmentMinutes = 90;    // 每个区间的行驶时间（分钟）
constexpr int kMaxLoginAttempts = 3;   // 登录最多尝试次数
constexpr int kMaxTicketsPerPhone = 3; // 每个手机号最多保存的有效票数

constexpr int SEAT_FREE = 0;      // 座位空闲
constexpr int SEAT_BOOKED = 1;    // 座位已预订
constexpr int SEAT_NONE = 2;      // 该区间没有这个座位（座位图缺省位）
constexpr int ORDER_ACTIVE = 0;   // 订单有效（未取消）
constexpr int ORDER_CANCELLED = 1;// 订单已取消

// 乘客账户
struct Account {
    std::string phone;        // 手机号，同时也是登录账号
    std::string password;     // 登录密码
    std::string name;         // 乘客姓名
    bool enabled = true;      // 是否启用，false 表示账号被停用
};

// 管理员账户
struct Admin {
    std::string account;      // 管理员账号
    std::string password;     // 管理员密码
    std::string name;         // 管理员名称
};

// 订单（车票）
struct Ticket {
    std::string id;           // 订单号，格式 YTyyyyMMdd-序号
    std::string date;         // 下单日期，格式 yyyy-MM-dd
    int busId = 0;            // 班次编号
    std::string phone;        // 乘客手机号
    std::string name;         // 乘客姓名
    int from = 0;             // 起点站编号
    int to = 0;               // 终点站编号
    int row = 0;              // 座位行号（从 0 开始）
    int col = 0;              // 座位列号（从 0 开始）
    int price = 0;            // 票价（元）
    int mask = 0;             // 占用的区间掩码，第 s 位为 1 表示占用第 s 个区间
    int status = ORDER_ACTIVE; // 订单状态：ORDER_ACTIVE 或 ORDER_CANCELLED
};

// 班次
struct Bus {
    int id = 0;               // 班次编号
    int hour = 0;             // 发车小时（0-23）
    int direction = 0;        // 行驶方向：0 表示 S->E，1 表示 E->S
    bool enabled = true;      // 是否运行，false 表示停运
    int seats[kSegments][kRows][kCols]{}; // 各区间、各行、各列的座位状态（SEAT_FREE/BOOKED/NONE）

    Bus(int busId = 0, int departureHour = 0, int dir = 0, bool on = true);
};

// 营收统计结果
struct RevenueSummary {
    int activeCount = 0;          // 未取消订单数
    int total = 0;                // 营业额合计（元）
    std::map<int, int> byBus;     // 按班次汇总：班次编号 -> 营业额
    std::map<int, int> byPrice;   // 按票价汇总：票面价格 -> 订单数
};

const char* stopName(int stop);
const char* directionName(int direction);
int stopAt(int direction, int position);
int stopPosition(int direction, int stop);
int segmentAtPosition(int direction, int position);
int priceByStops(int stops);
bool isValidPhone(const std::string& phone);
int systemNowMinutes();
std::string systemDateText();
std::string formatClock(int minutes);
std::string formatDuration(int minutes);

class TransitStore {
public:
    explicit TransitStore(std::filesystem::path dataDir);

    bool load(std::string& error);
    bool save(std::string& error) const;
    bool refreshForToday(std::string& error);
    /*
     * 备份数据（数据维护页）。
     * 参数：destination - 备份目录；error - 失败原因输出。
     * 返回值：四个数据文件全部复制成功返回 true，否则 false。
     */
    bool backup(const std::filesystem::path& destination, std::string& error) const;
    /*
     * 从备份恢复数据（数据维护页）。
     * 参数：source - 备份目录；error - 失败原因输出。
     * 返回值：备份完整、格式校验通过且覆盖成功后返回 true，否则 false。
     */
    bool restore(const std::filesystem::path& source, std::string& error);

    const std::vector<Bus>& buses() const { return buses_; }
    const std::vector<Account>& accounts() const { return accounts_; }
    const std::vector<Admin>& admins() const { return admins_; }
    const std::vector<Ticket>& orders() const { return orders_; }

    const Account* passengerLogin(const std::string& phone,
                                  const std::string& password) const;
    const Admin* adminLogin(const std::string& account,
                            const std::string& password) const;
    bool registerPassenger(const std::string& phone, const std::string& password,
                           const std::string& name, std::string& error);

    int seatStatus(const Bus& bus, int from, int to, int row, int col) const;
    int availableSeats(const Bus& bus, int from, int to) const;
    int wholeRouteFree(const Bus& bus) const;

    /*
     * 计算班次到达某个站的时刻（从 0 点开始的分钟数）。
     * 例如 6:00 发车、方向 S->E，则 S=360，A=450，B=540，E=630。
     * “随上随下”判断就靠它：只要当前时间还没到 from 站，就可以买票。
     */
    int arrivalMinutes(const Bus& bus, int stop) const;

    /*
     * 统计某手机号今天的有效订单数。
     * 参数：phone - 乘客手机号。
     * 返回值：今天且未取消的订单数量（用于限制每个账号最多 3 张有效票）。
     */
    int activeTicketsForPhone(const std::string& phone) const;
    /*
     * 统计某班次今天的有效订单数。
     * 参数：busId - 班次编号。
     * 返回值：今天且未取消的订单数量（停运、改方向、删除班次前都会检查）。
     */
    int activeTicketsForBus(int busId) const;
    /*
     * 统计某班次在指定日期的未取消订单数（班次管理表格第 5 列）。
     * 参数：busId - 班次编号；date - 日期，格式 yyyy-MM-dd。
     * 返回值：该班次在该日期且未取消的订单数量。
     */
    int ticketsForBusOnDate(int busId, const std::string& date) const;

    bool book(int busId, const std::string& phone, int from, int to,
              int row, int col, int nowMinutes,
              Ticket& created, std::string& error);
    /*
     * 取消订单（乘客本人或管理员）。
     * 参数：orderId - 订单号；phone - 乘客手机号（管理员取消时可为空）；
     *       adminOverride - true 表示管理员操作，可取消任意乘客的订单；
     *       error - 失败原因输出。
     * 返回值：成功返回 true；订单不存在、无权操作或已取消时返回 false。
     */
    bool cancelOrder(const std::string& orderId, const std::string& phone,
                     bool adminOverride, std::string& error);

    /*
     * 新增班次。
     * 参数：id - 班次编号；hour - 发车小时（0-23）；direction - 方向（0: S->E，1: E->S）；
     *       error - 失败原因输出。
     * 返回值：成功返回 true；编号重复或参数越界返回 false。
     */
    bool addBus(int id, int hour, int direction, std::string& error);
    /*
     * 修改班次发车时间。
     * 参数：id - 班次编号；hour - 新的发车小时（0-23）；error - 失败原因输出。
     * 返回值：班次存在且小时合法时返回 true，否则 false。
     */
    bool updateBus(int id, int hour, std::string& error);
    /*
     * 修改班次行驶方向。
     * 参数：id - 班次编号；direction - 新方向（0: S->E，1: E->S）；error - 失败原因输出。
     * 返回值：成功返回 true；班次不存在、方向非法或今天有有效订单时返回 false。
     */
    bool updateBusDirection(int id, int direction, std::string& error);
    /*
     * 启用/停运切换。
     * 参数：id - 班次编号；error - 失败原因输出。
     * 返回值：成功返回 true；班次不存在或运行中且有今天的有效订单时返回 false。
     */
    bool toggleBus(int id, std::string& error);
    /*
     * 删除班次。
     * 参数：id - 班次编号；error - 失败原因输出。
     * 返回值：成功返回 true；只剩一个班次、或有任何订单记录时返回 false。
     */
    bool deleteBus(int id, std::string& error);

    /*
     * 新增乘客账户。
     * 参数：account - 待新增的账户信息；error - 失败原因输出。
     * 返回值：成功返回 true；手机号非法、姓名/密码为空或手机号已存在时返回 false。
     */
    bool addAccount(const Account& account, std::string& error);
    /*
     * 删除乘客账户。
     * 参数：phone - 手机号；error - 失败原因输出。
     * 返回值：成功返回 true；账户不存在或存在今天的有效订单时返回 false。
     */
    bool deleteAccount(const std::string& phone, std::string& error);
    /*
     * 重置乘客密码。
     * 参数：phone - 手机号；password - 新密码（6-20 位、无空格）；error - 失败原因输出。
     * 返回值：成功返回 true；账户或新密码不合法时返回 false。
     */
    bool resetPassword(const std::string& phone, const std::string& password,
                       std::string& error);
    /*
     * 启用/停用乘客账户。
     * 参数：phone - 手机号；error - 失败原因输出。
     * 返回值：成功返回 true；账户不存在时返回 false。
     */
    bool toggleAccount(const std::string& phone, std::string& error);

    /*
     * 统计营收。
     * 参数：date - 统计日期（yyyy-MM-dd）；传空字符串表示统计全部日期。
     * 返回值：RevenueSummary，包含未取消订单数、营业额、按班次和按票价的分布。
     */
    RevenueSummary revenue(const std::string& date) const;

private:
    std::filesystem::path dataDir_;
    std::vector<Bus> buses_;
    std::vector<Account> accounts_;
    std::vector<Admin> admins_;
    std::vector<Ticket> orders_;

    Bus* findBus(int id);
    const Bus* findBus(int id) const;
    Ticket* findTicket(const std::string& id);
    Account* findAccount(const std::string& phone);
    const Account* findAccount(const std::string& phone) const;
    bool saveFile(const std::filesystem::path& path,
                  const std::string& text, std::string& error) const;
    bool loadTextFile(const std::filesystem::path& path,
                     std::string& text, std::string& error) const;
    bool loadBuses(std::string& error);
    bool loadAccounts(std::string& error);
    bool loadAdmins(std::string& error);
    bool loadOrders(std::string& error);
    void rebuildTodaySeats();
    std::string serializeBuses() const;
    std::string serializeAccounts() const;
    std::string serializeAdmins() const;
    std::string serializeOrders() const;
};

} // namespace transit
