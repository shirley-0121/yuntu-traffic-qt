# 第五版修复：演示时间 + 随上随下

## 1. 演示时间

查询栏新增：

- `使用演示时间` 复选框
- `HH:mm` 时间选择框（可上下调）
- `查询` 按钮

逻辑：

- 未勾选：使用 `transit::systemNowMinutes()`。
- 勾选：使用时间框里的“小时 * 60 + 分钟”作为计算时间。
- 日期仍然使用系统日期 `systemDateText()`，不受演示时间影响。
- 修改时间、勾选/取消勾选、点击查询都会重新计算班次和余座。
- 系统日期变化时仍然调用 `refreshForToday()`，清空并重建当天座位。

核心入口：

```cpp
int MainWindow::effectiveNowMinutes() const;
```

## 2. 随上随下

原来的逻辑是“发车后就不能买”。现在改成：

> 只要公交车还没到达乘客选择的起点站，就仍然可以买票。

新增：

```cpp
int TransitStore::arrivalMinutes(const Bus& bus, int stop) const;
```

到达某站的时刻 = 发车时刻 + 该站在行驶方向上的位置 × 90 分钟。

购票检查：

```cpp
if (nowMinutes >= arrivalMinutes(*bus, from)) {
    error = "班次已经到达或驶过起点站，停止预约";
    return false;
}
```

例子（1 号车 6:00 发车，S→E）：

| 当前时间 | 起点站 | 能否买 |
|---|---|---|
| 5:59 | S | 能 |
| 6:01 | S | 不能（已离开 S） |
| 7:00 | A | 能（还没到 A） |
| 7:30 | A | 不能（已到 A） |
| 8:50 | B | 能（还没到 B） |

界面上的班次状态也同步改成：

- 未发车：`下一站 X · 还有 Y`
- 已发车、未到起点站：`已发车 · 尚未到起点站（下一站 X · 还有 Y）`
- 已驶过起点站：`已驶过起点站 · 不可预约`
- 当日结束：`当日已结束`

座位图在“已驶过起点站”后会把空座标成“不可约”，`book()` 也会再检查一次。

## 3. 日期刷新

仍然保持第五版的机制：

- `load()` 时 `rebuildTodaySeats()`；
- `QTimer` 每秒检测系统日期；
- 日期变化调用 `refreshForToday()`：重置所有座位，再按“今天的有效订单”重新占座；
- 昨天的订单保留历史，但不占今天的座位。

## 4. 文件

- `mainwindow.h/.cpp`：演示时间控件、定时器、班次/座位状态、购票调用
- `transit_core.h/.cpp`：`arrivalMinutes()`、新的 `book(..., nowMinutes, ...)`
- `main.cpp`：入口不变
- `CMakeLists.txt` / `transit.pro`：构建配置（已改为实际文件名 `transit_core.*`）
- `bin/`：可直接运行的 Qt 程序、Qt DLL、platforms、data
- `preview.png`：08:50、从 B 站上车、1 号车已发车但未到 B 的界面

## 5. 运行

双击：

```text
bin\YuntuTrafficQt.exe
```

数据直接读写 **exe 同目录** 的：

```text
bin\data\passenger.txt
bin\data\bus.txt
bin\data\admin.txt
bin\data\orders.txt
```

新注册的账号会写进 `bin\data\passenger.txt`。注意 `QCoreApplication::applicationDirPath()` 是 exe 所在目录，不是源码目录；如果把 exe 复制到别处，要连同 `data` 一起复制。

测试账号：`13900000001 / 123456`，管理员 `admin / admin123`。

## 6. 重新编译

Qt Creator 打开 `transit.pro` 或 `CMakeLists.txt`，选 Qt 6.12 MinGW 64-bit 套件，构建目录放纯英文路径。

注意：原目录里 `transit_core .h/.cpp` 文件名带空格，而 `CMakeLists.txt` 写的是 `transit_core.h/.cpp`；本版本已经统一成不带空格的文件名，避免找不到源文件。
