#include "transit_core.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <numeric>
#include <sstream>

namespace transit {
namespace {

constexpr int kDefaultLayout[kRows][kCols] = {
    {SEAT_FREE, SEAT_FREE, SEAT_NONE, SEAT_FREE},
    {SEAT_FREE, SEAT_FREE, SEAT_NONE, SEAT_NONE},
    {SEAT_FREE, SEAT_FREE, SEAT_NONE, SEAT_FREE},
    {SEAT_FREE, SEAT_FREE, SEAT_NONE, SEAT_FREE},
    {SEAT_FREE, SEAT_FREE, SEAT_FREE, SEAT_FREE}
};

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::vector<std::string> split(const std::string& line, char separator) {
    std::vector<std::string> result;
    std::stringstream stream(line);
    std::string part;
    while (std::getline(stream, part, separator)) result.push_back(trim(part));
    return result;
}

bool integer(const std::string& text, int& value) {
    std::stringstream stream(trim(text));
    char extra = 0;
    return (stream >> value) && !(stream >> extra);
}

bool whitespace(const std::string& text) {
    return std::any_of(text.begin(), text.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    });
}

} // namespace

Bus::Bus(int busId, int departureHour, int dir, bool on)
    : id(busId), hour(departureHour), direction(dir), enabled(on) {
    for (int segment = 0; segment < kSegments; ++segment)
        for (int row = 0; row < kRows; ++row)
            for (int col = 0; col < kCols; ++col)
                seats[segment][row][col] = kDefaultLayout[row][col];
}

const char* stopName(int stop) {
    static constexpr const char* names[] = {
        "S 起点站", "A 中央公园", "B 科技园", "E 滨水站"
    };
    return (stop >= 0 && stop < kStops) ? names[stop] : "未知站点";
}

const char* directionName(int direction) { return direction == 0 ? "S → E" : "E → S"; }
int stopAt(int direction, int position) { return direction == 0 ? position : kStops - 1 - position; }
int stopPosition(int direction, int stop) { return direction == 0 ? stop : kStops - 1 - stop; }
int segmentAtPosition(int direction, int position) { return direction == 0 ? position : kSegments - 1 - position; }

int priceByStops(int stops) {
    if (stops <= 1) return 70;
    if (stops == 2) return 100;
    if (stops == 3) return 120;
    return 0;
}

bool isValidPhone(const std::string& phone) {
    return phone.size() == 11 && std::all_of(phone.begin(), phone.end(), [](unsigned char c) {
        return std::isdigit(c) != 0;
    });
}

int systemNowMinutes() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    return local.tm_hour * 60 + local.tm_min;
}

std::string systemDateText() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &local);
    return buffer;
}

std::string formatClock(int minutes) {
    std::ostringstream out;
    out << std::setfill('0') << std::setw(2) << minutes / 60 << ':'
        << std::setw(2) << minutes % 60;
    return out.str();
}

std::string formatDuration(int minutes) {
    if (minutes < 60) return std::to_string(minutes) + "分钟";
    const int hours = minutes / 60;
    const int rest = minutes % 60;
    return std::to_string(hours) + "小时" + (rest ? std::to_string(rest) + "分钟" : "");
}

TransitStore::TransitStore(std::filesystem::path dataDir)
    : dataDir_(std::move(dataDir)) {}

bool TransitStore::loadTextFile(const std::filesystem::path& path,
                                std::string& text, std::string& error) const {
    std::ifstream file(path, std::ios::binary);
    if (!file) { error = "无法打开数据文件：" + path.string(); return false; }
    std::ostringstream stream; stream << file.rdbuf(); text = stream.str(); return true;
}

bool TransitStore::saveFile(const std::filesystem::path& path,
                            const std::string& text, std::string& error) const {
    std::error_code ecDir;
    std::filesystem::create_directories(path.parent_path(), ecDir);
    std::filesystem::path temp = path;
    temp += ".tmp";
    { std::ofstream file(temp, std::ios::binary | std::ios::trunc);
      if (!file) { error = "无法写入：" + path.string(); return false; }
      file << text; if (!file.good()) { error = "写入失败：" + path.string(); return false; } }
    std::error_code ec; std::filesystem::remove(path, ec); std::filesystem::rename(temp, path, ec);
    if (ec) { error = "替换数据文件失败：" + path.string(); return false; }
    return true;
}

bool TransitStore::load(std::string& error) {
    if (!loadBuses(error) || !loadAccounts(error) || !loadAdmins(error) || !loadOrders(error)) {
        return false;
    }
    rebuildTodaySeats();
    return true;
}

bool TransitStore::save(std::string& error) const {
    return saveFile(dataDir_ / "bus.txt", serializeBuses(), error) &&
           saveFile(dataDir_ / "passenger.txt", serializeAccounts(), error) &&
           saveFile(dataDir_ / "admin.txt", serializeAdmins(), error) &&
           saveFile(dataDir_ / "orders.txt", serializeOrders(), error);
}

bool TransitStore::refreshForToday(std::string& error) {
    rebuildTodaySeats();
    return save(error);
}

/*
 * 备份数据（数据维护页）。
 * 参数：destination - 备份目录；error - 失败原因输出。
 * 返回值：四个数据文件全部复制成功返回 true，否则 false。
 */
bool TransitStore::backup(const std::filesystem::path& destination, std::string& error) const {
    std::error_code ec;
    std::filesystem::create_directories(destination, ec); // 备份目录不存在时自动创建
    if (ec) { error = "无法创建备份目录"; return false; }
    for (const char* name : {"bus.txt", "passenger.txt", "admin.txt", "orders.txt"}) {
        // 同名文件直接覆盖，形成一次完整快照
        std::filesystem::copy_file(dataDir_ / name, destination / name,
                                   std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) { error = "备份失败：" + std::string(name); return false; }
    }
    return true;
}

/*
 * 从备份恢复数据（数据维护页）。
 * 参数：source - 备份目录；error - 失败原因输出。
 * 返回值：备份完整、格式校验通过且覆盖成功后返回 true，否则 false。
 */
bool TransitStore::restore(const std::filesystem::path& source, std::string& error) {
    std::error_code ec;
    // 第一步：确认备份目录里四个数据文件齐全
    for (const char* name : {"bus.txt", "passenger.txt", "admin.txt", "orders.txt"}) {
        if (!std::filesystem::exists(source / name)) { error = "备份缺少：" + std::string(name); return false; }
    }
    // 第二步：先在临时对象里完整加载一遍，格式有问题就中止，避免破坏现有数据
    TransitStore candidate(source);
    if (!candidate.load(error)) return false;
    // 第三步：校验通过后覆盖当前数据文件，并重新加载到内存
    for (const char* name : {"bus.txt", "passenger.txt", "admin.txt", "orders.txt"}) {
        std::filesystem::copy_file(source / name, dataDir_ / name,
                                    std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) { error = "恢复失败：" + std::string(name); return false; }
    }
    return load(error);
}

bool TransitStore::loadBuses(std::string& error) {
    std::string text; if (!loadTextFile(dataDir_ / "bus.txt", text, error)) return false;
    buses_.clear(); std::istringstream input(text); std::string line; Bus* current = nullptr; bool segments[kSegments]{};
    auto finishBus = [&]() {
        if (!current) return true;
        for (bool loaded : segments) if (!loaded) { error = "bus.txt 座位区间数据不完整"; return false; }
        return true;
    };
    while (std::getline(input, line)) {
        line = trim(line); if (line.empty() || line[0] == '#') continue;
        std::istringstream row(line); std::string tag; row >> tag;
        if (tag == "BUS") { if (!finishBus()) return false; int id, hour, dir, enabled = 1; row >> id >> hour >> dir; if (row >> enabled) {}
            if (id <= 0 || hour < 0 || hour > 23 || (dir != 0 && dir != 1)) { error = "bus.txt 中 BUS 数据无效"; return false; }
            buses_.emplace_back(id, hour, dir, enabled != 0); current = &buses_.back(); std::fill(std::begin(segments), std::end(segments), false);
        } else if (tag == "SEG" && current) { int segment; row >> segment; if (segment < 0 || segment >= kSegments) { error = "bus.txt 中区间无效"; return false; }
            for (int r = 0; r < kRows; ++r) { if (!std::getline(input, line)) { error = "bus.txt 座位数据不完整"; return false; } std::istringstream seats(line); for (int c = 0; c < kCols; ++c) if (!(seats >> current->seats[segment][r][c])) { error = "bus.txt 座位数据无效"; return false; } }
            segments[segment] = true;
        }
    }
    if (!finishBus()) return false;
    if (buses_.empty()) { error = "没有可用班次"; return false; }
    return true;
}

bool TransitStore::loadAccounts(std::string& error) {
    std::string text; if (!loadTextFile(dataDir_ / "passenger.txt", text, error)) return false;
    accounts_.clear(); std::istringstream input(text); std::string line;
    while (std::getline(input, line)) { line = trim(line); if (line.empty() || line[0] == '#') continue; std::istringstream row(line); Account account; row >> account.phone >> account.password >> account.name; int enabled = 1; row >> enabled; account.enabled = enabled != 0; if (!isValidPhone(account.phone)) { error = "存在无效手机号"; return false; } accounts_.push_back(account); }
    if (accounts_.empty()) { error = "没有乘客账户"; return false; } return true;
}

bool TransitStore::loadAdmins(std::string& error) {
    std::string text; if (!loadTextFile(dataDir_ / "admin.txt", text, error)) return false;
    admins_.clear(); std::istringstream input(text); std::string line;
    while (std::getline(input, line)) { line = trim(line); if (line.empty() || line[0] == '#') continue; std::istringstream row(line); Admin admin; row >> admin.account >> admin.password >> admin.name; if (!admin.account.empty()) admins_.push_back(admin); }
    if (admins_.empty()) { error = "没有管理员账户"; return false; } return true;
}

bool TransitStore::loadOrders(std::string& error) {
    std::string text; if (!loadTextFile(dataDir_ / "orders.txt", text, error)) return false;
    orders_.clear(); std::istringstream input(text); std::string line;
    while (std::getline(input, line)) { line = trim(line); if (line.empty() || line[0] == '#') continue; auto f = split(line, ','); if (f.size() < 12) { error = "订单数据列数不足"; return false; } Ticket t; t.id=f[0]; t.date=f[1]; t.phone=f[3]; t.name=f[4]; if (!integer(f[2],t.busId)||!integer(f[5],t.from)||!integer(f[6],t.to)||!integer(f[7],t.row)||!integer(f[8],t.col)||!integer(f[9],t.price)||!integer(f[10],t.mask)||!integer(f[11],t.status)) { error = "订单数据无效"; return false; } orders_.push_back(t); }
    return true;
}

std::string TransitStore::serializeBuses() const { std::ostringstream out; out << "# bus.txt\n"; for (const auto& b : buses_) { out << "BUS "<<b.id<<' '<<b.hour<<' '<<b.direction<<' '<<(b.enabled?1:0)<<'\n'; for(int s=0;s<kSegments;++s){out<<"SEG "<<s<<'\n'; for(int r=0;r<kRows;++r){for(int c=0;c<kCols;++c){if(c)out<<' ';out<<b.seats[s][r][c];}out<<'\n';}}} return out.str(); }
std::string TransitStore::serializeAccounts() const { std::ostringstream out; for(const auto& a:accounts_) out<<a.phone<<' '<<a.password<<' '<<a.name<<' '<<(a.enabled?1:0)<<'\n'; return out.str(); }
std::string TransitStore::serializeAdmins() const { std::ostringstream out; for(const auto& a:admins_) out<<a.account<<' '<<a.password<<' '<<a.name<<'\n'; return out.str(); }
std::string TransitStore::serializeOrders() const { std::ostringstream out; out<<"# orders.txt\n"; for(const auto& t:orders_) out<<t.id<<','<<t.date<<','<<t.busId<<','<<t.phone<<','<<t.name<<','<<t.from<<','<<t.to<<','<<t.row<<','<<t.col<<','<<t.price<<','<<t.mask<<','<<t.status<<'\n'; return out.str(); }

void TransitStore::rebuildTodaySeats() {
    const std::string today = systemDateText();
    for (auto& bus : buses_) {
        for (int segment = 0; segment < kSegments; ++segment) {
            for (int row = 0; row < kRows; ++row) {
                for (int col = 0; col < kCols; ++col) {
                    bus.seats[segment][row][col] =
                        kDefaultLayout[row][col] == SEAT_NONE ? SEAT_NONE : SEAT_FREE;
                }
            }
        }
    }
    for (const auto& ticket : orders_) {
        if (ticket.status != ORDER_ACTIVE || ticket.date != today) continue;
        auto* bus = findBus(ticket.busId);
        if (!bus || ticket.row < 0 || ticket.row >= kRows ||
            ticket.col < 0 || ticket.col >= kCols) continue;
        const int p1 = stopPosition(bus->direction, ticket.from);
        const int p2 = stopPosition(bus->direction, ticket.to);
        if (p1 < 0 || p2 > kStops - 1 || p1 >= p2) continue;
        for (int p = p1; p < p2; ++p) {
            const int segment = segmentAtPosition(bus->direction, p);
            if (bus->seats[segment][ticket.row][ticket.col] != SEAT_NONE) {
                bus->seats[segment][ticket.row][ticket.col] = SEAT_BOOKED;
            }
        }
    }
}

const Account* TransitStore::findAccount(const std::string& phone) const { for(const auto& a:accounts_) if(a.phone==phone) return &a; return nullptr; }
Account* TransitStore::findAccount(const std::string& phone) { for(auto& a:accounts_) if(a.phone==phone) return &a; return nullptr; }
Bus* TransitStore::findBus(int id) { for(auto& b:buses_) if(b.id==id) return &b; return nullptr; }
const Bus* TransitStore::findBus(int id) const { for(const auto& b:buses_) if(b.id==id) return &b; return nullptr; }
Ticket* TransitStore::findTicket(const std::string& id) { for(auto& t:orders_) if(t.id==id) return &t; return nullptr; }

const Account* TransitStore::passengerLogin(const std::string& phone, const std::string& password) const { const auto* a=findAccount(phone); return a&&a->enabled&&a->password==password?a:nullptr; }
const Admin* TransitStore::adminLogin(const std::string& account, const std::string& password) const { for(const auto& a:admins_) if(a.account==account&&a.password==password) return &a; return nullptr; }

bool TransitStore::registerPassenger(const std::string& phone,const std::string& password,const std::string& name,std::string& error){ if(!isValidPhone(phone)){error="手机号必须为 11 位数字";return false;} if(password.size()<6||password.size()>20||whitespace(password)){error="密码长度必须为 6-20 位且不能含空格";return false;} if(name.empty()){error="姓名不能为空";return false;} if(findAccount(phone)){error="手机号已注册";return false;} accounts_.push_back({phone,password,name,true}); return save(error); }

int TransitStore::seatStatus(const Bus& bus,int from,int to,int row,int col) const { int p1=stopPosition(bus.direction,from),p2=stopPosition(bus.direction,to); for(int p=p1;p<p2;++p) if(bus.seats[segmentAtPosition(bus.direction,p)][row][col]==SEAT_BOOKED)return SEAT_BOOKED; for(int p=p1;p<p2;++p) if(bus.seats[segmentAtPosition(bus.direction,p)][row][col]==SEAT_NONE)return SEAT_NONE; return SEAT_FREE; }
int TransitStore::availableSeats(const Bus& bus,int from,int to) const { int count=0; for(int r=0;r<kRows;++r)for(int c=0;c<kCols;++c)if(seatStatus(bus,from,to,r,c)==SEAT_FREE)++count;return count; }
int TransitStore::wholeRouteFree(const Bus& bus) const { return availableSeats(bus, bus.direction==0?0:3, bus.direction==0?3:0); }
int TransitStore::arrivalMinutes(const Bus& bus, int stop) const {
    return bus.hour * 60 +
           stopPosition(bus.direction, stop) * kSegmentMinutes;
}

// 统计某手机号今天的有效订单数（phone：手机号；返回值：订单数量）
int TransitStore::activeTicketsForPhone(const std::string& phone) const {
    const auto today = systemDateText();             // 系统今天的日期
    return std::count_if(orders_.begin(), orders_.end(), [&](const Ticket& t) {
        return t.phone == phone && t.date == today && t.status == ORDER_ACTIVE;
    });
}
// 统计某班次今天的有效订单数（busId：班次编号；返回值：订单数量）
int TransitStore::activeTicketsForBus(int busId) const {
    const auto today = systemDateText();             // 系统今天的日期
    return std::count_if(orders_.begin(), orders_.end(), [&](const Ticket& t) {
        return t.busId == busId && t.date == today && t.status == ORDER_ACTIVE;
    });
}
// 统计某班次在指定日期的未取消订单数（busId：班次编号；date：yyyy-MM-dd；返回值：订单数量）
int TransitStore::ticketsForBusOnDate(int busId, const std::string& date) const {
    return std::count_if(orders_.begin(), orders_.end(), [&](const Ticket& t) {
        return t.busId == busId && t.date == date && t.status == ORDER_ACTIVE;
    });
}

bool TransitStore::book(int busId, const std::string& phone,
                        int from, int to, int row, int col,
                        int nowMinutes, Ticket& created,
                        std::string& error) {
    rebuildTodaySeats();

    Bus* bus = findBus(busId);
    if (bus == nullptr || !bus->enabled) {
        error = "班次不存在或已停运";
        return false;
    }

    // 随上随下：只要车还没到乘客选择的起点站，就还能买票。
    // 起点是始发站时，arrivalMinutes == 发车时间，发车后就不能再买。
    if (nowMinutes >= arrivalMinutes(*bus, from)) {
        error = "班次已经到达或驶过起点站，停止预约";
        return false;
    }

    if (activeTicketsForPhone(phone) >= kMaxTicketsPerPhone) {
        error = "每个手机号最多保留 3 张有效票";
        return false;
    }

    const int p1 = stopPosition(bus->direction, from);
    const int p2 = stopPosition(bus->direction, to);
    if (p1 >= p2 || row < 0 || row >= kRows ||
        col < 0 || col >= kCols) {
        error = "路线或座位无效";
        return false;
    }

    if (seatStatus(*bus, from, to, row, col) != SEAT_FREE) {
        error = "该座位已不可用";
        return false;
    }

    int mask = 0;
    for (int p = p1; p < p2; ++p) {
        const int segment = segmentAtPosition(bus->direction, p);
        bus->seats[segment][row][col] = SEAT_BOOKED;
        mask |= (1 << segment);
    }

    created.id = "YT";
    for (char c : systemDateText()) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            created.id += c;
        }
    }
    created.id += '-';

    int seq = 0;
    for (const auto& t : orders_) {
        if (t.id.rfind(created.id, 0) == 0) {
            seq = std::max(seq, std::atoi(t.id.c_str() + created.id.size()));
        }
    }
    std::ostringstream id;
    id << std::setfill('0') << std::setw(4) << seq + 1;
    created.id += id.str();

    created.date = systemDateText();
    created.busId = busId;
    created.phone = phone;
    const auto* account = findAccount(phone);
    created.name = account ? account->name : "乘客";
    created.from = from;
    created.to = to;
    created.row = row;
    created.col = col;
    created.price = priceByStops(p2 - p1);
    created.mask = mask;
    created.status = ORDER_ACTIVE;

    orders_.push_back(created);
    return save(error);
}

/*
 * 取消订单。
 * 参数：orderId - 订单号；phone - 乘客手机号（管理员操作时可为空）；
 *       adminOverride - true 表示管理员权限，可取消任意乘客的订单；
 *       error - 失败原因输出。
 * 返回值：成功返回 true；订单不存在、无权操作或已取消时返回 false。
 */
bool TransitStore::cancelOrder(const std::string& orderId, const std::string& phone,
                               bool adminOverride, std::string& error) {
    auto* t = findTicket(orderId);                   // 按订单号查找订单
    if (!t) { error = "订单不存在"; return false; }
    // 非管理员只能取消自己的订单
    if (!adminOverride && t->phone != phone) { error = "只能操作自己的订单"; return false; }
    if (t->status == ORDER_CANCELLED) { error = "订单已经取消"; return false; }

    auto* b = findBus(t->busId);                     // 订单对应的班次
    const bool today = t->date == systemDateText();  // 是否今天的订单
    t->status = ORDER_CANCELLED;                     // 先标记为已取消

    // 只有今天的订单才需要释放今天的座位；历史订单不占今天座位
    if (today && b) {
        for (int s = 0; s < kSegments; ++s) {        // s：区间编号
            if (!(t->mask & (1 << s))) continue;     // 该订单没占用这个区间则跳过
            // 检查同一座位、同一区间是否还有其他有效订单占用（订单可能只坐部分区间）
            bool covered = false;
            for (const auto& x : orders_) {
                if (x.id != t->id && x.status == ORDER_ACTIVE && x.date == t->date &&
                    x.busId == t->busId && x.row == t->row && x.col == t->col &&
                    (x.mask & (1 << s))) {
                    covered = true;
                    break;
                }
            }
            // 没有其他订单占用时，才把该区间座位恢复为空闲
            if (!covered) b->seats[s][t->row][t->col] = SEAT_FREE;
        }
    }
    return save(error);                              // 保存全部数据文件
}

/*
 * 新增班次。
 * 参数：id - 班次编号；hour - 发车小时（0-23）；direction - 方向（0: S->E，1: E->S）；
 *       error - 失败原因输出。
 * 返回值：成功返回 true；编号重复或参数越界返回 false。
 */
bool TransitStore::addBus(int id, int hour, int direction, std::string& error) {
    if (findBus(id)) { error = "班次编号已存在"; return false; }          // 编号不能重复
    if (id <= 0 || hour < 0 || hour > 23 ||
        direction < 0 || direction > 1) { error = "班次参数无效"; return false; } // 参数校验
    buses_.emplace_back(id, hour, direction, true);  // 新班次默认启用
    return save(error);                              // 保存数据文件
}

/*
 * 修改班次发车时间。
 * 参数：id - 班次编号；hour - 新的发车小时（0-23）；error - 失败原因输出。
 * 返回值：班次存在且小时合法时返回 true，否则 false。
 */
bool TransitStore::updateBus(int id, int hour, std::string& error) {
    auto* b = findBus(id);                           // 查找班次
    if (!b || hour < 0 || hour > 23) { error = "班次或时间无效"; return false; }
    b->hour = hour;                                  // 更新发车小时
    return save(error);
}

/*
 * 修改班次行驶方向。
 * 参数：id - 班次编号；direction - 新方向（0: S->E，1: E->S）；error - 失败原因输出。
 * 返回值：成功返回 true；班次不存在、方向非法或今天有有效订单时返回 false。
 */
bool TransitStore::updateBusDirection(int id, int direction, std::string& error) {
    auto* b = findBus(id);                           // 查找班次
    if (!b || direction < 0 || direction > 1) { error = "班次或方向无效"; return false; }
    // 今天已有有效订单时不允许改方向，否则座位与线路对不上
    if (activeTicketsForBus(id)) { error = "存在有效订单，不能修改方向"; return false; }
    b->direction = direction;                        // 更新方向
    return save(error);
}

/*
 * 启用/停运切换。
 * 参数：id - 班次编号；error - 失败原因输出。
 * 返回值：成功返回 true；班次不存在或运行中且有今天的有效订单时返回 false。
 */
bool TransitStore::toggleBus(int id, std::string& error) {
    auto* b = findBus(id);                           // 查找班次
    if (!b) { error = "班次不存在"; return false; }
    // 运行中的班次如果今天有有效订单，不能直接停运
    if (b->enabled && activeTicketsForBus(id)) { error = "存在有效订单，不能停运"; return false; }
    b->enabled = !b->enabled;                        // 切换启用状态
    return save(error);
}

/*
 * 删除班次。
 * 参数：id - 班次编号；error - 失败原因输出。
 * 返回值：成功返回 true；只剩一个班次、或有任何订单记录时返回 false。
 */
bool TransitStore::deleteBus(int id, std::string& error) {
    if (buses_.size() <= 1) { error = "至少保留一个班次"; return false; } // 至少保留一个班次
    // 只要该班次存在任何订单记录（含历史、含已取消），就不允许删除，避免订单悬空
    if (activeTicketsForBus(id) ||
        std::any_of(orders_.begin(), orders_.end(), [&](const Ticket& t) { return t.busId == id; })) {
        error = "该班次存在订单记录，不能删除";
        return false;
    }
    // 从班次列表中移除该班次
    auto it = std::remove_if(buses_.begin(), buses_.end(), [&](const Bus& b) { return b.id == id; });
    if (it == buses_.end()) { error = "班次不存在"; return false; }
    buses_.erase(it, buses_.end());
    return save(error);
}

/*
 * 新增乘客账户。
 * 参数：account - 待新增的账户信息；error - 失败原因输出。
 * 返回值：成功返回 true；手机号非法、姓名/密码为空或手机号已存在时返回 false。
 */
bool TransitStore::addAccount(const Account& account, std::string& error) {
    // 手机号必须是 11 位数字，姓名和密码不能为空
    if (!isValidPhone(account.phone) || account.name.empty() || account.password.empty()) {
        error = "账户信息无效";
        return false;
    }
    if (findAccount(account.phone)) { error = "手机号已存在"; return false; } // 手机号唯一
    accounts_.push_back(account);                    // 加入账户列表
    return save(error);
}

/*
 * 删除乘客账户。
 * 参数：phone - 手机号；error - 失败原因输出。
 * 返回值：成功返回 true；账户不存在或存在今天的有效订单时返回 false。
 */
bool TransitStore::deleteAccount(const std::string& phone, std::string& error) {
    // 今天仍有有效订单的乘客不能删除
    if (activeTicketsForPhone(phone)) { error = "账户存在有效订单，不能删除"; return false; }
    auto it = std::remove_if(accounts_.begin(), accounts_.end(),
                             [&](const Account& a) { return a.phone == phone; });
    if (it == accounts_.end()) { error = "账户不存在"; return false; }
    accounts_.erase(it, accounts_.end());            // 从列表中移除账户
    return save(error);
}

/*
 * 重置乘客密码。
 * 参数：phone - 手机号；password - 新密码（6-20 位、无空格）；error - 失败原因输出。
 * 返回值：成功返回 true；账户或新密码不合法时返回 false。
 */
bool TransitStore::resetPassword(const std::string& phone, const std::string& password,
                                 std::string& error) {
    auto* a = findAccount(phone);                    // 查找账户
    // 密码长度必须为 6-20 位且不含空格
    if (!a || password.size() < 6 || password.size() > 20 || whitespace(password)) {
        error = "账户或密码无效，密码需为 6-20 位";
        return false;
    }
    a->password = password;                          // 更新密码
    return save(error);
}

/*
 * 启用/停用乘客账户。
 * 参数：phone - 手机号；error - 失败原因输出。
 * 返回值：成功返回 true；账户不存在时返回 false。
 */
bool TransitStore::toggleAccount(const std::string& phone, std::string& error) {
    auto* a = findAccount(phone);                    // 查找账户
    if (!a) { error = "账户不存在"; return false; }
    a->enabled = !a->enabled;                        // 切换启用状态
    return save(error);
}

/*
 * 统计营收。
 * 参数：date - 统计日期（yyyy-MM-dd）；传空字符串表示统计全部日期。
 * 返回值：RevenueSummary，包含未取消订单数、营业额、按班次和按票价的分布。
 */
RevenueSummary TransitStore::revenue(const std::string& date) const {
    RevenueSummary result;                           // 统计结果，各字段初始为 0
    for (const auto& t : orders_) {                  // t：当前遍历到的订单
        // 只统计未取消订单；date 为空表示不限日期
        if (t.status != ORDER_ACTIVE || (!date.empty() && t.date != date)) continue;
        ++result.activeCount;                        // 订单数 +1
        result.total += t.price;                     // 营业额累加
        result.byBus[t.busId] += t.price;            // 按班次累加营业额
        result.byPrice[t.price] += 1;                // 按票价累加订单数
    }
    return result;
}

} // namespace transit
