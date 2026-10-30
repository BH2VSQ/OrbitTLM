#!/usr/bin/env python3
"""Merge all Chinese catalog/display strings into the embedded locale JSON files.

This maintenance tool preserves source labels in zh-CN.json and adds an English
counterpart to en-US.json. Exact, human-reviewed translations always win. For
new catalog labels it uses an engineering glossary plus catalog english_name
metadata, so future schema additions can be incorporated reproducibly.
"""
from __future__ import annotations
import json, re
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CJK = re.compile(r"[\u3400-\u9fff\uf900-\ufaff]")

# Longest and most specific terms first during composition. Values are UI English,
# not protocol identifiers; abbreviations and units are retained where useful.
GLOSSARY = {
    "升交点赤经":"Right Ascension of the Ascending Node", "近地点角距":"Argument of Perigee",
    "平近点角":"Mean Anomaly", "轨道倾角":"Orbital Inclination", "轨道偏心率":"Orbital Eccentricity",
    "OBC时间累计秒整数":"OBC Accumulated Time (Whole Seconds)", "OBC时间秒小数":"OBC Time Fractional Seconds",
    "UTC时间累计秒":"UTC Elapsed Seconds", "遥测主类型":"Telemetry Main Type", "遥测副类型":"Telemetry Subtype",
    "1PPS秒脉冲":"1PPS Pulse", "秒脉冲":"Pulse Per Second", "帧尾CBH":"Frame Trailer CBH",
    "帆板展开到位":"Solar Array Deployment Complete", "温度采集":"Temperature Sampling", "屏幕":"Screen",
    "分离装置":"Separation Mechanism", "偏振相机":"Polarization Camera", "星敏成像器件":"Star Tracker Imager",
    "星敏":"Star Tracker", "太敏":"Sun Sensor", "姿轨控":"Attitude and Orbit Control",
    "轨控":"Orbit Control", "姿控":"Attitude Control", "测控板":"TT&C Board", "测控":"TT&C",
    "卫星平台":"Satellite Platform", "星务":"Satellite Operations", "基带板":"Baseband Board",
    "AI板":"AI Board", "电源板":"Power Board", "核心板":"Core Board", "控制板":"Control Board",
    "通信板":"Communications Board", "接口板":"Interface Board", "测温":"Temperature Measurement",
    "冷推":"Cold-Gas Propulsion", "加热带":"Heater", "蓄电池":"Battery", "太阳阵":"Solar Array",
    "帆板":"Solar Array", "推力器":"Thruster", "飞轮":"Reaction Wheel", "星敏感器":"Star Tracker",
    "陀螺":"Gyroscope", "磁强计":"Magnetometer", "加速度计":"Accelerometer", "敏感器":"Sensor",
    "母线电压":"Bus Voltage", "母线电流":"Bus Current", "电池电压":"Battery Voltage",
    "总线电压":"Bus Voltage", "电压遥测":"Voltage Telemetry", "电流遥测":"Current Telemetry",
    "电压":"Voltage", "电流":"Current", "功耗":"Power Consumption", "功率":"Power",
    "温度上限":"Temperature Upper Limit", "温度下限":"Temperature Lower Limit", "上限":"Upper Limit", "下限":"Lower Limit",
    "温度":"Temperature", "湿度":"Humidity", "压力":"Pressure", "电量":"Charge Level",
    "复位计数":"Reset Count", "复位记数":"Reset Count", "执行指令计数":"Executed Command Count",
    "错误指令计数":"Invalid Command Count", "发送计数":"Transmit Count", "接收计数":"Receive Count",
    "无线发送":"Wireless Transmit", "无线接收":"Wireless Receive", "无线":"Wireless",
    "正确帧计数":"Valid Frame Count", "错误帧计数":"Error Frame Count", "错误计数":"Error Count",
    "计数器":"Counter", "计数":"Count", "记数":"Count", "有效计数":"Valid Count",
    "发送":"Transmit", "接收":"Receive", "发射":"Transmit", "下传":"Downlink", "上行":"Uplink",
    "下行":"Downlink", "上注":"Uplink", "遥控":"Telecommand", "遥测":"Telemetry",
    "指令标识":"Command Identifier", "指令代号":"Command ID", "指令":"Command", "命令":"Command",
    "传输长度":"Transfer Length", "数据长度":"Data Length", "文件大小":"File Size", "字节":"Bytes",
    "上注文件":"Uplink File", "SD卡":"SD Card", "TF卡":"TF Card", "存储卡":"Memory Card",
    "软件版本号":"Software Version", "协议版本":"Protocol Version", "版本号":"Version",
    "标识":"Identifier", "标记":"Flag", "标志":"Flag", "状态":"Status", "模式":"Mode",
    "当前值":"Current Value", "当前":"Current", "最近":"Latest", "上次":"Previous",
    "累计":"Cumulative", "总计":"Total", "总":"Total", "已用":"Used", "剩余":"Remaining",
    "启动标志":"Startup Flag", "启动":"Startup", "停止":"Stop", "运行时间":"Uptime",
    "运行":"Runtime", "工作":"Operation", "工作模式":"Operating Mode", "安全模式":"Safe Mode",
    "自主":"Autonomous", "就绪":"Ready", "异常":"Fault", "故障":"Fault", "错误":"Error",
    "正确":"Valid", "有效":"Valid", "无效":"Invalid", "成功":"Success", "失败":"Failure",
    "检测":"Detection", "锁定":"Lock", "锁定检测":"Lock Detection", "载波":"Carrier",
    "同步":"Synchronization", "帧同步":"Frame Synchronization", "校验和":"Checksum", "校验":"Validation",
    "帧尾":"Frame Trailer", "帧头":"Frame Header", "帧类型":"Frame Type", "帧序号":"Frame Sequence",
    "包序号":"Packet Sequence", "序号":"Sequence Number", "包":"Packet", "分包":"Packet Segment",
    "数据包":"Data Packet", "数据帧":"Data Frame", "帧":"Frame", "保留":"Reserved",
    "位宽":"Bit Width", "字节序":"Byte Order", "小端":"Little-endian", "大端":"Big-endian",
    "年":"Year", "月":"Month", "日":"Day", "时":"Hour", "分":"Minute", "秒":"Second",
    "时间-年":"Time - Year", "时间-月":"Time - Month", "时间-日":"Time - Day",
    "时间-时":"Time - Hour", "时间-分":"Time - Minute", "时间-秒":"Time - Second",
    "计算机时间":"Computer Time", "UTC时间":"UTC Time", "遥测时间":"Telemetry Time", "时间":"Time",
    "GPS速度X":"GPS Velocity X", "GPS速度Y":"GPS Velocity Y", "GPS速度Z":"GPS Velocity Z",
    "GPS速度":"GPS Velocity", "GPS轨道":"GPS Orbit", "轨道":"Orbit", "速度X":"Velocity X",
    "速度Y":"Velocity Y", "速度Z":"Velocity Z", "速度":"Velocity", "加速度":"Acceleration",
    "四元数":"Quaternion", "欧拉角":"Euler Angle", "角速度":"Angular Rate", "角度":"Angle",
    "偏心率":"Eccentricity", "半长轴":"Semi-major Axis", "倾角":"Inclination", "赤经":"Right Ascension",
    "近地点":"Perigee", "远地点":"Apogee", "轨道根数":"Orbital Elements", "定位结果":"Positioning Result",
    "解算模式":"Solution Mode", "定轨":"Orbit Determination", "定姿":"Attitude Determination",
    "算法参数来源":"Algorithm Parameter Source", "算法":"Algorithm", "参数来源":"Parameter Source",
    "参数":"Parameter", "使用":"Use", "来源":"Source", "输出":"Output", "输入":"Input",
    "上电次数":"Power-on Count", "开机计数":"Boot Count", "开机":"Power-on", "关机":"Shutdown",
    "过流":"Overcurrent", "欠压":"Undervoltage", "过压":"Overvoltage", "过放保护":"Over-discharge Protection",
    "放电":"Discharge", "充电":"Charge", "负载":"Load", "开关状态":"Switch State", "开关":"Switch",
    "电源控制":"Power Control", "电源":"Power", "供电":"Power Supply", "温控":"Thermal Control",
    "风扇":"Fan", "转速":"Rotational Speed", "频率":"Frequency", "频率-MHz":"Frequency (MHz)",
    "CPU使用率":"CPU Usage", "使用率":"Utilization", "CPU":"CPU", "GPU":"GPU", "温度-摄氏度":"Temperature (°C)",
    "摄氏度":"°C", "百分比":"Percent", "无效":"Invalid", "寄存器":"Register", "脚本":"Script",
    "目录":"Directory", "文件":"File", "流程":"Process", "结果":"Result", "阶段":"Stage",
    "工作目录":"Working Directory", "根分区":"Root Partition", "剩余空间":"Free Space", "累计秒":"Elapsed Seconds",
    "运行时间":"Runtime", "耗时":"Elapsed Time", "等待耗时":"Wait Time", "错误码":"Error Code",
    "退出码":"Exit Code", "PID":"PID", "低字节":"Low Byte", "高字节":"High Byte", "低16位":"Low 16 Bits",
    "长度":"Length", "数量":"Count", "字":"Word", "主版本":"Major Version", "次版本":"Minor Version",
    "修订版本":"Revision", "接收错误":"Receive Error", "发送错误":"Transmit Error", "CAN总线":"CAN Bus",
    "总线":"Bus", "总线任务":"Bus Task", "超时":"Timeout", "握手":"Handshake", "所有权":"Ownership",
    "看门狗":"Watchdog", "复位原因":"Reset Cause", "软件复位":"Software Reset", "电源使能":"Power Enable",
    "使能":"Enable", "禁用":"Disable", "刷新请求":"Refresh Request", "待处理":"Pending",
    "记录":"Record", "保存":"Save", "已保存":"Saved", "丢弃":"Discarded", "处理":"Process",
    "检查":"Check", "诊断":"Diagnostics", "报告":"Report", "格式化":"Format", "格式错误":"Format Error",
    "访问":"Access", "拷贝":"Copy", "回写":"Write-back", "最近一次":"Most Recent",
    "可用":"Available", "累计上电次数":"Cumulative Power-on Count", "介质错误":"Media Error",
    "寿命消耗":"Life Consumption", "非安全关机":"Unsafe Shutdown", "错误日志":"Error Log",
    "数字人":"Avatar", "形象":"Image", "问题编号":"Issue ID", "回答":"Response", "问题提交":"Issue Submission",
    "网络状态":"Network Status", "以太网":"Ethernet", "串口":"Serial Port", "通信状态":"Communication Status",
    "通信":"Communication", "打开状态":"Open Status", "配置状态":"Configuration Status",
    "同步字":"Sync Word", "总状态":"Overall Status", "最近指令":"Latest Command", "复位前":"Before Reset",
    "活动":"Active", "当前活动":"Currently Active", "卡槽":"Card Slot", "槽":"Slot", "挂载失败":"Mount Failure",
    "持久化":"Persistent", "故障事件":"Fault Event", "事件类型":"Event Type", "任务":"Task",
    "固件":"Firmware", "Flash写入":"Flash Write", "允许":"Allowed", "BOR复位":"BOR Reset",
    "NRST引脚":"NRST Pin", "引脚":"Pin", "低功耗管理":"Low-power Management", "独立看门狗":"Independent Watchdog",
    "窗口看门狗":"Window Watchdog", "内部温度":"Internal Temperature", "运行时间-s":"Runtime (s)",
    "外部":"External", "内部":"Internal", "保留字段":"Reserved Field", "消息":"Message", "标识符":"Identifier",
    "秒脉冲发送标识":"PPS Transmit Identifier", "发送标识":"Transmit Identifier", "信噪比":"Signal-to-Noise Ratio",
    "最高信噪比":"Maximum Signal-to-Noise Ratio", "捕获星数":"Acquired Satellite Count", "星数":"Satellite Count",
    "电池温度":"Battery Temperature", "结构板":"Structural Panel", "温度采集":"Temperature Sampling",
    "冯相机":"Feng Camera", "冷推测温":"Cold-Gas Propulsion Temperature Measurement", "储箱":"Tank",
    "展开到位":"Deployment Complete", "到位":"In Position", "帆板展开":"Solar Array Deployment",
    "天线":"Antenna", "相机":"Camera", "图像":"Image", "小相机":"Mini Camera", "热控":"Thermal Control",
    "参数标识":"Parameter Identifier", "任务指向":"Mission Pointing", "指向":"Pointing", "姿态更新":"Attitude Update",
    "可信标志":"Validity Flag", "安全模式累计时间":"Cumulative Safe-mode Time", "常值漂移":"Constant Bias Drift",
    "Gyrodrift":"Gyro Drift", "使用GPS测量确定轨道":"Use GPS Measurements for Orbit Determination",
    "A总线":"A Bus", "B总线":"B Bus", "C总线":"C Bus", "D总线":"D Bus", "正确帧":"Valid Frame",
    "发送错误":"Transmit Error", "接收错误":"Receive Error", "电源控制状态":"Power Control Status",
    "数据门":"Data Gate", "核心板":"Core Board", "CMOS":"CMOS", "ISP":"ISP", "上行工作模式":"Uplink Operating Mode",
    "数传速率":"Data Rate", "过流检测":"Overcurrent Detection", "载波锁定":"Carrier Lock",
    "数据缓存":"Data Buffer", "相机时间来源":"Camera Time Source", "内部指令":"Internal Command",
    "过温":"Overtemperature", "控制器":"Controller", "码同步锁定":"Code Synchronization Lock",
    "就绪状态":"Ready Status", "单机":"Standalone Unit", "核心":"Core", "负载电流":"Load Current",
    "数据":"Data", "消息":"Message", "消息-":"Message -", "电压mV":"Voltage (mV)", "电流mA":"Current (mA)",
    "上限":"Upper Limit", "下限":"Lower Limit", "A机":"Unit A", "B机":"Unit B", "IO电压":"I/O Voltage",
    "IO电流":"I/O Current", "核心电压":"Core Voltage", "核心电流":"Core Current",
    "长":"Long", "短":"Short", "次镜":"Secondary Mirror", "承力":"Load-bearing", "焦面电箱":"Focal-plane Electronics Box",
    "分离装置":"Separation Mechanism", "上注状态":"Uplink Status", "数据帧":"Data Frame", "帧尾":"Frame Trailer",
    "调试":"Debug", "状态位":"Status Bit", "编号":"ID", "序号":"Sequence", "保留位":"Reserved Bits",
}


# Additional terms found in the full BY04/ASRTU-1 official catalog. Keep these
# specific phrases ahead of shorter terms so compound telemetry names remain readable.
GLOSSARY.update({
    "遥测轮询计数":"Telemetry Poll Count", "微型相机":"Miniature Camera", "微型":"Miniature",
    "小相机":"Mini Camera", "照片编号":"Photo ID", "照片":"Photo", "请求照片":"Photo Request",
    "FLASH参数":"FLASH Parameters", "FLASH照片":"FLASH Photo", "回读":"Readback", "读回":"Readback",
    "遥测慢包":"Slow Telemetry Packet", "慢包":"Slow Packet", "快包":"Fast Packet", "快遥测":"Fast Telemetry",
    "地面":"Ground", "地面上注":"Ground Uplink", "地面发送":"Ground Transmit", "保护倒计时":"Protection Countdown",
    "保护时长":"Protection Duration", "保护":"Protection", "应用错误":"Application Error", "应用":"Application",
    "重启计数":"Restart Count", "重启":"Restart", "重新启动":"Restart", "开启计数":"Enable Count",
    "关闭计数":"Disable Count", "开启":"Enable", "关闭":"Disable", "打开":"Open", "关闭状态":"Off Status",
    "六根数":"Six Orbital Elements", "平根数":"Mean Orbital Elements", "六根数SixEA":"Six Orbital Elements SixEA",
    "余弦矩阵":"Direction Cosine Matrix", "方向余弦矩阵":"Direction Cosine Matrix", "部件极性校正":"Component Polarity Correction",
    "磁力矩器":"Magnetorquer", "磁力矩器控制":"Magnetorquer Control", "磁棒":"Magnetic Rod", "通讯":"Communication",
    "载噪比":"Carrier-to-Noise Ratio", "波束":"Beam", "短报文":"Short Message", "报文":"Message",
    "偏差姿态":"Attitude Deviation", "偏差":"Deviation", "最大允许":"Maximum Allowed", "最大应用错误":"Maximum Application Errors",
    "最大重启":"Maximum Restarts", "最大":"Maximum", "最小":"Minimum", "实际":"Actual", "期望":"Expected",
    "未收到":"Not Received", "未接收":"Not Received", "健康管理方式":"Health Management Mode", "管理方式":"Management Mode",
    "变换":"Transformation", "地固变换":"Earth-fixed Transformation", "地固":"Earth-fixed", "地心太阳":"Geocentric Sun",
    "地心飞行器":"Geocentric Spacecraft", "地心":"Geocentric", "飞行器位置":"Spacecraft Position", "飞行器":"Spacecraft",
    "指定地点在载荷坐标系中的":"Specified Location in Payload Coordinate Frame", "目标坐标系偏置":"Target Frame Offset",
    "目标定向三轴稳定偏置":"Target-pointing Three-axis Stabilization Bias", "定向三轴稳定偏置":"Three-axis Pointing Bias",
    "单位向量分量":"Unit-vector Components", "向量分量":"Vector Components", "向量":"Vector", "球坐标位置":"Spherical-coordinate Position",
    "角动量":"Angular Momentum", "系统角动量":"System Angular Momentum", "本体角速率":"Body Angular Rate",
    "相对轨道角速率":"Relative Orbital Angular Rate", "角速率":"Angular Rate", "角速度":"Angular Velocity",
    "角速度漂移":"Angular-rate Drift", "漂移":"Drift", "角动量上界":"Angular Momentum Upper Bound",
    "控制比例补偿系数":"Proportional Control Compensation Coefficient", "控制比例系数":"Proportional Control Coefficient",
    "控制积分系数":"Integral Control Coefficient", "控制微分系数":"Derivative Control Coefficient",
    "比例补偿系数":"Proportional Compensation Coefficient", "比例系数":"Proportional Coefficient",
    "积分系数":"Integral Coefficient", "微分系数":"Derivative Coefficient", "系数":"Coefficient",
    "变化界":"Change Limit", "偏移量":"Offset", "偏置角":"Bias Angle", "偏置":"Bias", "偏差姿态统计值":"Attitude Deviation Statistics",
    "统计值":"Statistics", "安装四元数":"Mounting Quaternion", "安装":"Mounting", "解锁":"Unlock",
    "加电开关":"Power Switch", "自主加电":"Autonomous Power-on", "开关通":"Switch On", "开关断":"Switch Off",
    "加电":"Power-on", "关机保护":"Shutdown Protection", "关机":"Shutdown", "开机":"Startup", "关开启":"Off/On",
    "开关闭":"On/Off", "开开启":"On/On", "关关闭":"Off/Off", "开":"On", "关":"Off", "通":"On", "断":"Off",
    "接通":"Connected", "连通":"Connected", "在线":"Online", "启用":"Enabled", "使能":"Enable", "不可取消":"Non-cancellable",
    "底板":"Backplane", "部件":"Component", "部件使用标志":"Component-use Flag", "使用标志":"Usage Flag",
    "占空比":"Duty Cycle", "增益":"Gain", "角动量上界":"Angular Momentum Upper Bound", "输出角动量":"Output Angular Momentum",
    "最后一次设置值":"Last Set Value", "设置值":"Set Value", "设置":"Set", "设置指令":"Configuration Command",
    "复位前":"Before Reset", "检测到":"Detected", "BOR复位":"BOR Reset", "NRST引脚":"NRST Pin",
    "活动CAN":"Active CAN", "当前活动":"Current Active", "所有权属于":"Owned by", "属于":"Belongs to",
    "切换等待":"Switching Wait", "卡槽":"Card Slot", "卡槽切换":"Card-slot Switch", "挂载失败":"Mount Failure",
    "持久化记录中":"In Persistent Log", "持久化故障事件":"Persistent Fault Event", "程序状态":"Program Status",
    "进程状态":"Process Status", "最近执行":"Most Recent Execution", "当前操作":"Current Operation", "文件名":"File Name",
    "一键流程":"One-click Workflow", "流程阶段":"Workflow Stage", "流程结果":"Workflow Result", "程序进入次数":"Program Entry Count",
    "程序正常进入":"Normal Program Entry", "程序进入":"Program Entry", "运行数字人":"Running Avatar", "数字人":"Avatar",
    "本次运行时间":"Current Run Time", "等待耗时":"Wait Duration", "时间戳":"Timestamp", "毫秒时间戳":"Millisecond Timestamp",
    "曝光时间":"Exposure Time", "曝光补偿":"Exposure Compensation", "灰度均值":"Mean Gray Level", "亮度":"Brightness",
    "锐度":"Sharpness", "压缩率":"Compression Ratio", "文件大小":"File Size", "偏移量":"Offset", "分区表头":"Partition Table Header",
    "文件分区表头":"File Partition Table Header", "文件偏移":"File Offset", "文件索引":"File Index", "最后包索引":"Last Packet Index",
    "转存":"Transfer to Storage", "上注转存":"Uplink-to-storage Transfer", "报文编号":"Message Number", "指令帧序号":"Command Frame Sequence",
    "频偏估计":"Frequency Offset Estimate", "物理层译码率":"Physical-layer Decode Rate", "译码率":"Decode Rate",
    "冗余率":"Redundancy Ratio", "FEC冗余率":"FEC Redundancy Ratio", "自动遥测间隔":"Automatic Telemetry Interval",
    "告警标识":"Alarm Identifier", "告警":"Alarm", "消息":"Message", "载荷长度":"Payload Length", "载荷":"Payload",
    "地址":"Address", "寄存器地址":"Register Address", "寄存器值":"Register Value", "读寄存器":"Read Register", "写寄存器":"Write Register",
    "模块ID":"Module ID", "模块":"Module", "单点定位":"Single-point Positioning", "位置误差限度":"Position Error Limit",
    "位置":"Position", "定位":"Positioning", "定位结果":"Positioning Result", "GPS定位":"GPS Positioning", "输出平根数":"Output Mean Orbital Elements",
    "变化界":"Change Limit", "上界":"Upper Bound", "下界":"Lower Bound", "阈值":"Threshold", "最低电压阈值":"Minimum Voltage Threshold",
    "飞行程序":"Flight Software", "延时指令":"Delayed Command", "指令执行时刻":"Command Execution Time", "延时":"Delay",
    "对日定向":"Sun Pointing", "对地任务":"Earth-pointing Mission", "目标类型":"Target Type", "任务目标":"Mission Target",
    "定向偏置角":"Pointing Bias Angle", "稳定偏置":"Stabilization Bias", "快速机动":"Rapid Maneuver", "机动等效":"Maneuver-equivalent",
    "等效角速度":"Equivalent Angular Velocity", "递推方法":"Propagation Method", "轨道递推":"Orbit Propagation", "轨道姿态角":"Orbit Attitude Angle",
    "姿态角":"Attitude Angle", "姿态":"Attitude", "载荷坐标系":"Payload Coordinate Frame", "坐标系":"Coordinate Frame",
    "地球表面交点":"Earth-surface Intersection", "与地球表面交点":"Earth-surface Intersection with", "阴影因子":"Shadow Factor",
    "环境力矩":"Environmental Torque", "光学敏感器":"Optical Sensor", "一致性检测":"Consistency Check", "灰度":"Grayscale",
    "观测星个数":"Observed Star Count", "识别星个数":"Identified Star Count", "捕获星数":"Acquired Satellite Count",
    "烧写扇区":"Programmed Sector", "FLASH参数":"FLASH Parameters", "照片长度":"Photo Length", "正在发送":"Currently Transmitting",
    "请求参数":"Request Parameters", "标志位":"Flag Bit", "通道板":"Channel Board", "开机计数":"Boot Count", "开机计数低字节":"Boot Count Low Byte",
    "片选":"Chip Select", "数据门状态":"Data Gate Status", "网口状态":"Ethernet Status", "上行遥控指令错误原因":"Uplink Telecommand Error Cause",
    "最大应用错误计数":"Maximum Application Error Count", "最大重启计数":"Maximum Restart Count", "上界":"Upper Bound", "最近一条":"Latest",
    "最近一次":"Most Recent", "每次":"Per Event", "有效标记":"Validity Flag", "时间来源":"Time Source", "脉冲发送标识":"Pulse Transmit Identifier",
    "有效":"Valid", "无":"No", "是否":"Whether", "存在":"Present", "程序":"Program", "中断":"Interrupt", "在线状态":"Online Status",
    "传感器":"Sensor", "压力传感器":"Pressure Sensor", "电磁阀":"Solenoid Valve", "磁场强度":"Magnetic Field Strength",
    "耦合变换":"Coupled Transformation", "飞行器位置":"Spacecraft Position", "地球固定":"Earth-fixed", "星下点推扫":"Nadir Pushbroom",
    "调速星下点推扫":"Speed-adjusted Nadir Pushbroom", "单点定位结果":"Single-point Positioning Result", "参数请求":"Parameter Request",
    "控制器过流":"Controller Overcurrent", "过温计数":"Overtemperature Count", "加热带温度":"Heater Temperature",
    "保留":"Reserved", "Reserved":"Reserved", "遥测时间":"Telemetry Time", "计算机":"Computer", "本体":"Body",
})


# Additional subsystem-specific vocabulary observed in catalog labels.
GLOSSARY.update({
    "请求照片编号":"Photo Request ID", "微型相机请求":"Miniature Camera Request", "照片编号":"Photo ID",
    "BINTXT":"BIN-to-TXT", "BIN转TXT":"BIN-to-TXT", "CPU_GPU_CV功耗":"CPU/GPU/CV Power Consumption",
    "文件系统类型":"File System Type", "TF卡文件系统":"TF Card File System", "系统类型":"System Type",
    "输入文件":"Input File", "输出目录":"Output Directory", "工作目录":"Working Directory", "根分区":"Root Partition",
    "内存":"Memory", "交换区":"Swap Space", "总内存":"Total Memory", "总状态":"Overall Status",
    "介质错误次数":"Media Error Count", "错误日志条数":"Error Log Entry Count", "非安全关机次数":"Unsafe Shutdown Count",
    "平均过流":"Average Overcurrent", "瞬时过流":"Instantaneous Overcurrent", "事件计数":"Event Count", "次数":"Count",
    "主版本":"Major Version", "次版本":"Minor Version", "修订版本":"Revision", "协议版本":"Protocol Version",
    "功耗模式":"Power Mode", "模式调整":"Mode Adjustment", "守护程序":"Daemon", "安全关机":"Safe Shutdown",
    "工作流程":"Workflow", "最近诊断":"Latest Diagnostics", "故障诊断":"Fault Diagnostics", "诊断准备状态":"Diagnostics Ready Status",
    "启动结果":"Startup Result", "目录状态":"Directory Status", "启动结果目录":"Startup Result Directory",
    " Avatar":" Avatar", "当前或最近运行":"Currently or Most Recently Running", "最近或当前":"Latest or Current",
    "回答等待":"Response Wait", "回答等待耗时":"Response Wait Duration", "数字人准备":"Avatar Readiness",
    "当前运行":"Current Runtime", "本次运行":"Current Run", "已保存":"Saved", "已用内存":"Used Memory",
    "已用交换区":"Used Swap Space", "累计上电":"Cumulative Power-on", "最近启动":"Latest Startup",
    "电压阈值":"Voltage Threshold", "电源状态":"Power Status", "实际电源状态":"Actual Power Status", "期望电源状态":"Desired Power Status",
    "磁强计通讯":"Magnetometer Communication", "通讯有效":"Communication Valid", "有效标记":"Validity Flag", "时间戳有效":"Timestamp Valid",
    "六根数":"Six Keplerian Elements", "六根数Six":"Six Keplerian Elements Six", "平根数":"Mean Orbital Elements",
    "余弦矩阵":"Direction Cosine Matrix", "平均值":"Mean Value", "均值":"Mean Value", "数值":"Numeric Value",
    "矫正":"Correction", "校正":"Correction", "磁场强度":"Magnetic Field Strength", "反馈":"Feedback", "期望":"Desired",
    "译码":"Decode", "译码率":"Decode Rate", "子帧":"Subframe", "物理层":"Physical Layer", "扩展队列":"Extension Queue",
    "持续外推":"Continuous Extrapolation", "外推时间":"Extrapolation Time", "最大允许无遥测":"Maximum Allowed Telemetry Gap",
    "应用错误计数":"Application Error Count", "应用错误":"Application Error", "最大应用":"Maximum Application",
    "重启":"Restart", "最大重启":"Maximum Restarts", "健康管理":"Health Management", "管理方式":"Management Method",
    "变换":"Transformation", "耦合":"Coupling", "指定地点":"Specified Location", "坐标系中的":"in Coordinate Frame",
    "单位向量":"Unit Vector", "分量":"Component", "量的球坐标位置":"Spherical-coordinate Position Components",
    "输出平根数":"Output Mean Orbital Elements", "定位异常":"Positioning Anomaly", "定位位置":"Position Position",
    "变化界":"Change Bound", "偏移":"Offset", "偏移量":"Offset", "偏置":"Bias", "角动量":"Angular Momentum",
    "环境力矩":"Environmental Torque", "惯性":"Inertial", "本体角速率":"Body Angular Rate", "系统角动量":"System Angular Momentum",
    "六根数":"Six Orbital Elements", "MeanStderr":"Mean Standard Error", "载噪比":"Carrier-to-Noise Ratio",
    "波束":"Beam", "保护倒计时":"Protection Countdown", "倒计时":"Countdown", "开启计数":"Enable Count", "关闭计数":"Disable Count",
    "开关开启":"Switch Enabled", "开关关闭":"Switch Disabled", "开关闭计数":"Switch-off Count", "开开启计数":"Switch-on Count",
    "计数低字节":"Count Low Byte", "编号":"ID", "标志":"Flag", "通道板":"Channel Board", "中断状态":"Interrupt Status",
    "地面参数":"Ground Parameter", "参数设置":"Parameter Configuration", "请求指令":"Request Command", "指令错误原因":"Command Error Cause",
    "连续":"Continuous", "延时":"Delay", "偏置角":"Bias Angle", "对日":"Sun-pointing", "对地":"Earth-pointing",
    "目标":"Target", "星下点":"Subsatellite Point", "推扫":"Pushbroom Scanning", "调速":"Speed-adjusted", "太阳":"Sun",
    "地球":"Earth", "地固":"Earth-fixed", "飞行器":"Spacecraft", "卫星":"Satellite", "飞行程序":"Flight Software",
    "定向":"Pointing", "机动":"Maneuver", "定向偏置角":"Pointing Bias Angle", "快速机动放大系数":"Rapid Maneuver Scale Factor",
    "角速率":"Angular Rate", "角速度":"Angular Velocity", "系数":"Coefficient", "加权矩阵":"Weighting Matrix",
    "分配加权矩阵":"Allocation Weighting Matrix", "加权":"Weighted", "递推":"Propagation", "选用序号":"Selection Index",
    "安装":"Mounting", "成像器件":"Imaging Device", "曝光":"Exposure", "灰度均值":"Mean Grayscale", "识别星":"Identified Star",
    "观测星":"Observed Star", "观测星个数":"Observed Star Count", "锐度":"Sharpness", "亮度":"Brightness",
    "压缩率":"Compression Ratio", "程序正常进入":"Normal Program Entry", "上次":"Previous", "最近":"Latest",
    "回读":"Readback", "读回":"Readback", "通讯有效":"Communication Valid", "串口帧":"Serial Frame", "累加和":"Checksum",
    "看门狗":"Watchdog", "闪存":"Flash Memory", "烧写":"Programming", "扇区":"Sector", "起始地址":"Start Address",
    "程序进入":"Program Entry", "进入次数":"Entry Count", "缓存":"Buffer", "扩展":"Extended", "队列":"Queue",
    "网络":"Network", "在线":"Online", "模块ID":"Module ID", "源":"Source", "数据源":"Data Source", "重启状态":"Restart Status",
    "复位原因":"Reset Cause", "最早":"Earliest", "最大值":"Maximum Value", "最近生成":"Latest Generated",
    "像素格式":"Pixel Format", "文件分区":"File Partition", "文件偏移量":"File Offset", "偏移":"Offset", "大小":"Size",
    "地址":"Address", "值":"Value", "读":"Read", "写":"Write", "保护":"Protection", "启用":"Enable",
    "通道":"Channel", "标记":"Flag", "固定":"Fixed", "变更":"Change", "授时":"Time Synchronization",
    "授时方式":"Time Synchronization Method", "本地时间":"Local Time", "毫秒":"Millisecond", "秒脉冲发送标识":"PPS Transmit Identifier",
    "最小":"Minimum", "最大":"Maximum", "标定":"Calibration", "参数校准":"Parameter Calibration", "磁棒":"Magnetic Rod",
    "占空比":"Duty Cycle", "输出":"Output", "输入":"Input", "方向":"Direction", "极性":"Polarity", "部件极性校正":"Component Polarity Correction",
    "参数修改":"Parameter Change", "生效":"Active", "自动增益":"Automatic Gain", "放大系数":"Scale Factor", "信号":"Signal",
    "心跳":"Heartbeat", "存活计数":"Alive Count", "任务间隔":"Task Interval", "间隔时间":"Interval", "当前像素":"Current Pixel",
    "图片大小":"Image Size", "生成的图片":"Generated Image", "折叠部件":"Foldable Component", "展开选择":"Deployment Selection",
    "部件使用":"Component Use", "执行时刻":"Execution Time", "上一":"Previous", "下一":"Next", "开始":"Start", "结束":"End",
})


# Canonical translations for recurring compound labels where generic composition
# would be awkward.
GLOSSARY.update({
    "冷推测温":"Cold-Gas Propulsion Temperature Measurement", "单位向量分量":"Unit-vector Components",
    "未收到遥测时长":"Telemetry Timeout", "遥测时长":"Telemetry Duration", "最大允许无遥测计数":"Maximum Allowed Telemetry Gap Count",
    "可下发遥测包标识":"Telemetry Packet Available for Downlink Identifier", "自身遥测下发标识":"Self Telemetry Downlink Identifier",
    "自身遥测下发模式":"Self Telemetry Downlink Mode", "自身遥测下发计数":"Self Telemetry Downlink Count",
    "自身遥测下发频度":"Self Telemetry Downlink Rate", "自身遥测下发":"Self Telemetry Downlink", "可下发":"Available for Downlink",
    "实时遥测剩余时间":"Real-time Telemetry Remaining Time", "接收到":"Received", "接收中心机":"Receive at the Central Unit",
    "工程参数请求指令":"Engineering Parameter Request Command", "工程参数":"Engineering Parameters", "参数设置":"Parameter Configuration",
    "参数配置":"Parameter Configuration", "终端参数配置指令":"Terminal Parameter Configuration Command",
    "接收到地面发送的参数设置指令":"Received Ground Parameter Configuration Command", "对应起讫时间":"Corresponding Start/End Time",
    "起讫时间":"Start/End Time", "轨道参数对应":"Orbital Parameter Corresponding", "遥测包校验":"Telemetry Packet Validation",
    "间隔遥测次数":"Telemetry Count per Interval", "短报文遥测间隔":"Short-message Telemetry Interval", "遥测间隔":"Telemetry Interval",
    "自动增益状态":"Automatic Gain Status", "生效的增益":"Active Gain", "生效的曝光":"Active Exposure",
    "已使用寿命":"Used Lifetime", "内部存储已使用寿命":"Internal Storage Used Lifetime", "心跳存活计数":"Heartbeat Alive Count",
    "扩展队列":"Extended Queue", "上注数据源":"Uplink Data Source", "最新存盘":"Latest Saved Record",
    "待处理":"Pending", "目标类型":"Target Type", "传输到":"Transfer to", "中心机":"Central Unit", "当班机":"Active Unit",
    "基带UV":"Baseband UV", "基带HW":"Baseband HW", "地面发送的":"Sent by Ground", "参数修改标记":"Parameter Change Flag",
    "自动遥测":"Automatic Telemetry", "遥测发送":"Telemetry Transmit", "遥测接收":"Telemetry Receive", "遥测下发":"Telemetry Downlink",
    "在载荷坐标系中的":"in the Payload Coordinate Frame", "球坐标":"Spherical Coordinates", "矢量":"Vector", "分量":"Components",
    "时间戳有效标志":"Timestamp Validity Flag", "曝光时间":"Exposure Time", "文件大小":"File Size", "文件偏移量":"File Offset",
    "请求照片":"Photo Request", "请求照片编号":"Photo Request ID", "请求照片参数":"Photo Request Parameters",
    "校验状态":"Validation Status", "累加和错误计数":"Checksum Error Count", "串口帧累加和":"Serial Frame Checksum",
    "复位累计次数":"Cumulative Reset Count", "硬件复位累计次数":"Cumulative Hardware Reset Count",
    "软件版本":"Software Version", "扩展数据":"Extended Data", "数据有效":"Data Valid", "信号强度":"Signal Strength",
})


# Catalog labels that occur frequently as standalone values or as the final
# token of a compound name.
GLOSSARY.update({
    "基带":"Baseband", "蓄电池温度":"Battery Temperature", "蓄电池电压":"Battery Voltage",
    "光点个数":"Light Spot Count", "包头":"Packet Header", "包类型":"Packet Type", "包序号":"Packet Sequence",
    "北斗用户卡号":"BeiDou User Card Number", "基带切换原因":"Baseband Switch Cause", "多普勒":"Doppler",
    "已拍张数":"Captured Photo Count", "总张数":"Total Photo Count", "显示类别":"Display Category",
    "波特率":"Baud Rate", "系统秒":"System Seconds", "网口协商速率":"Ethernet Negotiated Link Rate", "频段":"Frequency Band",
    "错误包":"Error Packet", "上注错误包":"Uplink Error Packet", "已收到总包数":"Total Packets Received",
    "总包数":"Total Packet Count", "剩余包数":"Remaining Packet Count", "当前包索引":"Current Packet Index", "包索引":"Packet Index",
    "继电器":"Relay", "继电器状态":"Relay Status", "确定故障":"Confirmed Fault", "确定故障标志":"Confirmed Fault Flag",
    "计算机确定":"Computer-identified", "推进":"Propulsion", "控制流程":"Control Workflow", "控制":"Control",
    "需计算":"Calculation Required", "类型":"Type", "桥电流":"Bridge Current", "数字":"Digital", "惯组":"Inertial Measurement Unit",
    "选择":"Selection", "测量":"Measurement", "数传":"Data Link", "数传开关":"Data Link Switch", "执行":"Execution",
    "最后指令":"Last Command", "指令执行结果":"Command Execution Result", "写PL寄存器":"Write PL Register",
    "读PL寄存器":"Read PL Register", "寄存器":"Register", "值":"Value", "槽":"Slot", "加热":"Heating",
    "高清相机":"HD Camera", "高清":"HD", "分离":"Separation", "禁止":"Prohibited", "母线":"Bus",
    "结构矩阵":"Structural Matrix", "帧数":"Frame Count", "帧处理时间":"Frame Processing Time", "测试":"Test",
    "已处理":"Processed", "已成功保存":"Successfully Saved", "已完成":"Completed", "已收到":"Received",
    "无遥控":"No Telecommand", "无遥控时间":"Time Since Last Telecommand", "无遥控计数":"No-telecommand Count",
    "幅值":"Amplitude", "存储":"Storage", "门限":"Threshold", "监控":"Monitoring", "继电器开启":"Relay On",
    "继电器关闭":"Relay Off", "自动开启":"Automatic Enable", "自动拍照":"Automatic Photo Capture", "开启标志":"Enable Flag",
    "展开":"Deployment", "展开状态":"Deployment Status", "扩展":"Extended", "选择模式":"Selection Mode",
    "瞬时":"Instantaneous", "数传接收":"Data Link Receive", "测量角速度":"Measured Angular Velocity", "数据采集":"Data Acquisition",
    "代码":"Code", "卡号":"Card Number", "卡号标识":"Card Number Identifier", "卡号第一字节":"Card Number First Byte",
    "卡号后四字节":"Card Number Last Four Bytes", "Doppler":"Doppler", "GNSS":"GNSS", "射频":"RF", "继电器":"Relay",
    "惯组陀螺":"IMU Gyroscope", "光纤陀螺":"Fiber-optic Gyroscope", "重新启动":"Restart", "基带复位":"Baseband Reset",
    "根数":"Orbital Elements", "日":"Day", "年":"Year", "月":"Month", "时":"Hour", "分钟":"Minute",
    "周内秒":"Seconds of Week", "GPS周":"GPS Week", "自2017起":"Since 2017", "自J2000起":"Since J2000",
    "已处理1024字节":"Processed 1024 Bytes", "已成功保存1024字节":"Successfully Saved 1024 Bytes",
    "最近诊断异常帧数":"Latest Diagnostic Invalid Frame Count", "最近诊断检查帧数":"Latest Diagnostic Checked Frame Count",
    "最近诊断故障条数":"Latest Diagnostic Fault Count", "最近诊断故障类型":"Latest Diagnostic Fault Type",
    "最新存盘":"Latest Saved Record", "当前所用":"Currently Used", "所使用的总线":"Bus in Use", "当前所用总线":"Current Bus in Use",
    "基带软件升级":"Baseband Software Upgrade", "最近动作":"Latest Action", "能源管理":"Energy Management", "动作":"Action",
    "已配置":"Configured", "链路建立":"Link Established", "工作是否正常":"Operating Normally", "模块运行中":"Module Running",
    "超视场":"Out of Field of View", "测试模式":"Test Mode", "单点定位门限":"Single-point Positioning Threshold",
    "单点定轨":"Single-point Orbit Determination", "融合异常":"Fusion Anomaly", "输出变化界":"Output Change Limit",
    "门限状态":"Threshold Status", "接收频偏":"Receive Frequency Offset", "自动曝光":"Automatic Exposure", "曝光状态":"Exposure Status",
    "图像灰度":"Image Grayscale", "星点质心提取":"Star Centroid Extraction", "异常标志":"Anomaly Flag",
    "平场校正":"Flat-field Correction", "抗杂光功能":"Stray-light Rejection Function", "识别星个数":"Identified Star Count",
    "参数来源":"Parameter Source", "参数请求":"Parameter Request", "系统毫秒":"System Milliseconds", "相机时间":"Camera Time",
    "图像状态":"Image Status", "信源模式":"Source Mode", "服务重启次数":"Service Restart Count", "启动计秒数":"Seconds Since Startup",
    "存活计数":"Alive Count", "累计脉冲计数":"Cumulative Pulse Count", "触发时间":"Trigger Time", "跳秒":"Leap Second",
    "频点":"Frequency Channel", "吞吐":"Throughput", "链路":"Link", "光纤":"Fiber-optic", "质量":"Mass",
    "推进剂当前质量":"Current Propellant Mass", "飞轮饱和计数上界":"Reaction Wheel Saturation Count Upper Bound",
    "自动遥测间隔":"Automatic Telemetry Interval", "队列溢出计数":"Queue Overflow Count", "格式":"Format", "截断":"Truncated",
    "有效时长":"Valid Duration", "轮询年龄":"Polling Age", "收敛":"Convergence", "太阳捕获":"Sun Acquisition",
    "地面交点":"Ground Intersection", "入射角余弦":"Cosine of Incidence Angle", "指令码":"Command Code", "最后一条":"Last",
    "正常进入":"Normal Entry", "接收字节数":"Received Byte Count", "日志字节数":"Log Byte Count", "已拍":"Captured",
    "图片大小":"Image Size", "系统时间":"System Time", "持久化":"Persistent", "安全模式触发":"Safe-mode Trigger",
    "剩余时间":"Remaining Time", "启动姿态":"Startup Attitude", "提前调整时间":"Advance Adjustment Time", "拍照分辨率":"Photo Resolution",
    "画面平均亮度":"Mean Image Brightness", "本包":"This Packet", "文件名类别":"File Name Category", "保存照片源编号":"Saved Photo Source ID",
    "主备机标识":"Primary/Backup Unit Identifier", "数传网卡":"Data Link Network Adapter", "在线状态":"Online Status",
    "队列长度":"Queue Length", "监控状态":"Monitoring Status", "状态显示":"Status Display", "最新生成的图片大小":"Latest Generated Image Size",
    "写入状态":"Write Status", "请求功耗模式":"Requested Power Mode", "最近错误代号":"Latest Error ID", "Fault":"Fault",
})


# Human-reviewed full-label overrides for remaining catalog edge cases.
GLOSSARY.update({
    "日志与数据帧文件路径":"LOG/BIN session input paths", "日志文件：%1\n数据帧文件：%2":"Log file: %1\nFrame file: %2", "已断开连接":"Disconnected",
    "两个数字人启动脚本状态":"Startup Status of Both Avatar Scripts", "功耗模式调整状态":"Power Mode Adjustment Status",
    "守护程序状态":"Daemon Status", "数字人最近启动结果代号":"Latest Avatar Startup Result Code",
    "数字人本次运行时间":"Avatar Current Run Time", "距最近报告时间":"Time Since Latest Report",
    "距最近数字人回答时间":"Time Since Latest Avatar Response", "距最近遥控时间":"Time Since Latest Telecommand",
    "距最近问题提交时间":"Time Since Latest Issue Submission", "遥控拒绝计数":"Rejected Telecommand Count",
    "Orin故障码":"Orin Fault Code", "SD遥测记录正在进行":"SD Telemetry Recording Active", "NDP握手已完成":"NDP Handshake Complete",
    "活动CAN为":"Active CAN Is", "活动卡槽为":"Active Card Slot Is", "活动SD卡槽为":"Active SD Card Slot Is",
    "读卡器刷新待处理":"Card Reader Refresh Pending", "已处理1024字节TM记录计数":"Processed 1024-byte TM Record Count",
    "已成功保存1024字节TM记录计数":"Successfully Saved 1024-byte TM Record Count", "持久化记录中":"In Persistent Log",
    "检测到":"Detected", "检测到NRST引脚复位标志":"NRST Pin Reset Detected Flag",
    "检测到POR或PDR复位标志":"POR or PDR Reset Detected Flag", "检测到低功耗管理复位标志":"Low-power Management Reset Detected Flag",
    "检测到内部独立看门狗复位标志":"Internal Independent Watchdog Reset Detected Flag",
    "检测到内部窗口看门狗复位标志":"Internal Window Watchdog Reset Detected Flag",
    "B1：CMOS和ISP电源控制状态":"B1: CMOS and ISP Power Control Status", "B2：星地遥控数据缓存状态":"B2: Space-to-ground Telecommand Buffer Status",
    "B5：码组":"B5: Code Group", "B6：发射机工作模式":"B6: Transmitter Operating Mode", "发射机工作模式":"Transmitter Operating Mode",
    "B7~4：发射机过流计数":"B7–4: Transmitter Overcurrent Count", "BDS参与解算星数":"BDS Satellites Used in Solution",
    "GPS参与解算星数":"GPS Satellites Used in Solution", "Bash运行终止":"Bash Process Terminated", "上注设备选择":"Uplink Device Selection",
    "FRAM已发送长度":"FRAM Transmitted Length", "GPS周内秒":"GPS Seconds of Week", "GPS输出平根数越界检测使能":"Enable Out-of-range Mean Orbital Element Detection",
    "OBC总电流":"OBC Total Current", "PDOP小于8":"PDOP Below 8", "STM32最后外部指令":"Last External STM32 Command",
    "STM32系统运行时间戳":"STM32 System Runtime Timestamp", "UT1时间预测":"UT1 Time Prediction", "UTC累积秒整秒":"UTC Accumulated Whole Seconds",
    "UTC累计秒整数":"UTC Accumulated Integer Seconds", "if9361发射功率增益温补":"IF9361 Transmit Power Gain Temperature Compensation",
    "ul_prach符号偏移周期统计":"UL PRACH Symbol Offset Period Statistics", "上注全检标志":"Full Uplink Verification Flag",
    "与偏振相机连通状态":"Polarization Camera Connectivity Status", "与数传单机连通状态":"Data Link Unit Connectivity Status",
    "主程序区校验":"Main Program Area Checksum", "从启动计秒数":"Seconds Since Startup", "以太网恢复计数":"Ethernet Recovery Count",
    "以太网速率(Mbps)":"Ethernet Link Speed (Mbps)", "使能健康能源管理":"Enable Health and Energy Management",
    "使能太阳翼展开":"Enable Solar Wing Deployment", "保留尾部":"Reserved Tail", "偏流载荷偏流角PianLiuAngle":"Payload Cross-track Bias Angle PianLiuAngle",
    "内存总量(MiB)":"Total Memory (MiB)", "冯相机任务开始文件号":"Feng Camera Task Start File Number",
    "功率告警卫星收到的入站功率":"Power Alarm: Incoming Power Received by Satellite", "功率告警调整功率值":"Power Alarm Adjusted Power Value",
    "北斗使用CAN":"BeiDou Uses CAN", "单点定位迭代次数":"Single-point Positioning Iteration Count",
    "单音信号使能标志":"Single-tone Signal Enable Flag", "单音信号频率":"Single-tone Signal Frequency",
    "卫星时间自J2000起天计数TimeDate2000":"Satellite Days Since J2000 TimeDate2000",
    "卫星本体调偏流依据载荷选择PayLoadPianliu_Select":"Payload Selection for Satellite Body Cross-track Bias Correction",
    "参与滤波星数":"Number of Satellites Used in Filtering", "坏点校正状态":"Bad Pixel Correction Status",
    "基带下发计数":"Baseband Downlink Count", "备用 3.3V电压":"Backup 3.3 V Voltage", "备用 3.3V电流":"Backup 3.3 V Current",
    "大角速度干扰补偿系数UsingTorqureCross":"High Angular-rate Disturbance Compensation Coefficient UsingTorqureCross",
    "太阳偏流载荷地面交点入射角余弦cosSunIncidentAngle":"Cosine of Incidence Angle at Ground Intersection of Sun-offset Payload cosSunIncidentAngle",
    "姿态模式持续时间":"Attitude Mode Duration", "定姿方法标记":"Attitude Determination Method Flag",
    "对目标指向姿态连续最长时间MissionUpTime":"Longest Continuous Target-pointing Attitude Duration MissionUpTime",
    "开机累计超24小时标志":"Powered-on for More Than 24 Hours Flag", "归一化阈值":"Normalization Threshold",
    "当机序号":"Active Unit Sequence Number", "最新生成的图片大小":"Latest Generated Image Size",
    "相机时间：年":"Camera Time: Year", "相机时间：月":"Camera Time: Month", "相机时间：日":"Camera Time: Day",
    "相机时间：时":"Camera Time: Hour", "相机时间：分":"Camera Time: Minute", "相机连拍状态":"Camera Burst Capture Status",
    "短报文计算crc":"Short-message CRC Calculation", "磁盘剩余(MiB)":"Disk Free Space (MiB)", "磁盘可用":"Disk Available",
    "系统对基带选择模式":"System Baseband Selection Mode", "终端内缓存遥控指令数":"Telecommands Buffered in Terminal",
    "自主加电开关断":"Autonomous Power Switch Off", "自主加电开关通":"Autonomous Power Switch On",
    "自主加电开关断-回读":"Autonomous Power Switch Off — Readback", "自主加电开关通-回读":"Autonomous Power Switch On — Readback",
    "自动黑电平状态":"Automatic Black Level Status", "计算机理论控制周期Tsample":"Computer Theoretical Control Period Tsample",
    "贮箱温度输出Tktank":"Tank Temperature Output Tktank", "转发出遥控指令计数":"Forwarded Telecommand Count",
    "轮询次数":"Poll Count", "软件上注状态显示":"Software Uplink Status Display", "进入测试模式标志":"Test Mode Entry Flag",
    "进入自控程序次数":"Autonomous Control Program Entry Count", "连拍间隔(x10ms)":"Burst Capture Interval (×10 ms)",
    "遥控解析计数":"Telecommand Parse Count", "锁定波束号":"Locked Beam Number", "错误指令原因":"Invalid Command Cause",
    "闭环星敏感器选用序号FlagAscSelect":"Closed-loop Star Tracker Selection Index FlagAscSelect",
    "闭环磁强计选用序号FlagMAGBSelect":"Closed-loop Magnetometer Selection Index FlagMAGBSelect",
    "闭环陀螺选用序号FlagIMUWSelect":"Closed-loop Gyroscope Selection Index FlagIMUWSelect",
    "需计算指定地点在载荷坐标系分量的标号PayLoadCompute_Select":"Selection Index for Specified Location Components in Payload Frame PayLoadCompute_Select",
    "需计算指定地点在载荷坐标系分量的球坐标位置":"Spherical-coordinate Position of Specified Location Components in Payload Frame",
    "非法遥控数据包计数":"Invalid Telecommand Packet Count", "飞行程序延时-展帆":"Flight Software Delay — Solar Wing Deployment",
    "首次加电标志":"First Power-on Flag", "热刀时间":"Thermal Knife Time", "能源管理最近动作":"Latest Energy Management Action",
    "模块已配置":"Module Configured", "模块运行中":"Module Running", "本地自维持时间":"Local Holdover Duration",
    "使能自动拍照":"Enable Automatic Photo Capture", "自动黑电平":"Automatic Black Level", "最近生成的图片大小":"Latest Generated Image Size",
    "相机时间":"Camera Time", "当前所用总线":"Currently Used Bus", "当前所用基带":"Currently Used Baseband",
    "星时跳变监控开启标志":"Enable Star-time Jump Monitoring Flag", "自动曝光状态":"Automatic Exposure Status",
    "相机时间授时标记状态":"Camera Time Synchronization Flag Status", "当前像素格式":"Current Pixel Format",
    "写PL寄存器0地址":"Write PL Register 0 Address", "写PL寄存器0值":"Write PL Register 0 Value",
    "读PL寄存器1地址":"Read PL Register 1 Address", "读PL寄存器1值":"Read PL Register 1 Value",
    "主程序区":"Main Program Area", "数据采集外发标志":"Data Acquisition Forwarding Flag", "时间连续性标识":"Time Continuity Identifier",
    "软件上注":"Software Uplink", "进入测试模式":"Enter Test Mode", "光纤陀螺输出上界":"Fiber-optic Gyroscope Output Upper Bound",
    "光纤陀螺输出变化界":"Fiber-optic Gyroscope Output Change Limit", "延时指令执行时刻":"Delayed Command Execution Time",
    "自动增益状态":"Automatic Gain Status", "生效的黑电平(x100)":"Active Black Level (×100)", "滤波后sigma":"Filtered Sigma",
})


def cjk_strings(value):
    out=set()
    if isinstance(value,str):
        if CJK.search(value): out.add(value)
    elif isinstance(value,dict):
        for k,v in value.items():
            if isinstance(k,str) and CJK.search(k): out.add(k)
            out |= cjk_strings(v)
    elif isinstance(value,list):
        for v in value: out |= cjk_strings(v)
    return out


def translate_compound(source: str, exact: dict[str,str], candidates: dict[str,list[str]]) -> str:
    direct=GLOSSARY.get(source,"")
    if direct and not CJK.search(direct):
        return direct
    existing=exact.get(source,"")
    if existing and not CJK.search(existing):
        return existing
    english_options=[x for x in candidates.get(source,[]) if x and not CJK.search(x)]
    if english_options:
        # Prefer short, general field names over packet-specific annotations.
        return sorted(set(english_options), key=lambda x:(len(x), x.count(":"), x))[0]
    result=source
    merged=dict(GLOSSARY)
    # Consume any complete hyphenated source phrase first. Protect the generated
    # English replacement from being mistaken for an original separator.
    protected=[]
    for k in sorted((k for k in merged if len(k)>1 and "-" in k), key=len, reverse=True):
        if k in result:
            value=" " + merged[k] + " "
            token=f"\uE100{len(protected)}\uE101"
            protected.append((token,value))
            result=result.replace(k,token)
    # Remaining hyphens came from the source/catalog and act as visual separators.
    result=result.replace("-", "\uE000")
    for k in sorted((k for k in merged if len(k)>1 and "-" not in k), key=len, reverse=True):
        if k in result:
            result=result.replace(k, " " + merged[k] + " ")
    safe_single={"包":"Packet", "值":"Value", "开":"On", "关":"Off", "秒":"Seconds", "数":"Count", "头":"Header", "槽":"Slot", "桥":"Bridge", "帧":"Frame", "读":"Read", "写":"Write"}
    for k,v in safe_single.items():
        if k in result: result=result.replace(k, " " + v + " ")
    for token,value in protected:
        result=result.replace(token,value)
    result=result.replace("\uE000", " — ")
    result=re.sub(r"\s+", " ", result).strip()
    result=re.sub(r"\s*([,:;])\s*", r"\1 ", result)
    result=re.sub(r"\s+([\)\] ])", r"\1", result)
    result=re.sub(r"([\[(])\s+", r"\1", result)
    repairs=[
        ("Telemetry slow packet","Slow Telemetry Packet"),
        ("Telemetry main type","Telemetry Main Type"),
        ("Telemetry Sub Type","Telemetry Subtype"),
        ("Telemetry Subtype00H","Telemetry Subtype 0x00"),
        ("Power-on Count low byte","Power-on Count (Low Byte)"),
        ("Little — endian","Little-endian"),
        ("Cold — Gas","Cold-Gas"),
        ("Unit — vector","Unit-vector"),
    ]
    for a,b in repairs: result=result.replace(a,b)
    # No source-language glyphs are allowed to leak into English UI, even for
    # mission-specific names that are not yet covered by the domain glossary.
    if CJK.search(result):
        result=re.sub(r"[\u3400-\u9fff\uf900-\ufaff]+", "Parameter", result)
        result=re.sub(r"\s+", " ", result).strip(" -—:;_")
        result=(result or "Telemetry Parameter")
    return result


def main():
    zh_path=ROOT/"i18n/zh-CN.json"; en_path=ROOT/"i18n/en-US.json"
    zh=json.loads(zh_path.read_text(encoding="utf-8")); en=json.loads(en_path.read_text(encoding="utf-8"))
    zh_map=zh["translations"]; en_map=en["translations"]
    # Keep the source labels and candidate English metadata from all active catalogs.
    candidates=defaultdict(list)
    strings=set()
    for path in sorted((ROOT/"telemetry-trees").glob("*.json")):
        data=json.loads(path.read_text(encoding="utf-8"))
        strings |= cjk_strings(data)
        for packet in data.get("packets",[]):
            for field in packet.get("fields",[]):
                name=field.get("name",""); english=field.get("english_name","")
                if isinstance(name,str) and isinstance(english,str) and name and english and not CJK.search(english):
                    candidates[name].append(english)
    # Include all existing Chinese keys so locale maps remain complete and symmetric.
    strings |= {k for k in zh_map if CJK.search(k)}
    strings |= {k for k in zh.get("webTranslations",{}) if CJK.search(k)}
    for source in sorted(strings):
        zh_map.setdefault(source, source)
        en_map.setdefault(source, translate_compound(source,en_map,candidates))
        if CJK.search(en_map[source]):
            en_map[source]=translate_compound(source,{},candidates)
    # Save predictable, reviewable UTF-8 JSON, preserving the nested locale format.
    zh["translations"]={k:zh_map[k] for k in sorted(zh_map)}
    en["translations"]={k:en_map[k] for k in sorted(en_map)}
    zh_path.write_text(json.dumps(zh,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    en_path.write_text(json.dumps(en,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    residual=[(k,v) for k,v in en_map.items() if CJK.search(v)]
    print(f"Catalog source strings: {len(strings)}; locale entries: {len(en_map)}; remaining CJK translations: {len(residual)}")
    if residual: print(residual[:20])

if __name__=="__main__": main()
