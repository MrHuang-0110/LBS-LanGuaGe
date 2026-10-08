#include "asr.h"
extern "C"{ void * __dso_handle = 0 ;}
#include "setup.h"
#include "HardwareSerial.h"
#include "myLib/asr_event.h"

// ============================================================
// 小白-Ai UART 协议 v0.7 (ASRPRO 端)
// MCU → ASR: play=<id>\n / stop\n / ping\n     (id: 17-41)
// ASR → MCU: cmd=<id>\r\n / wake\r\n / done=<id>\r\n
//   cmd id: 1-16, wake 对应识别 ID=0
// 9600 8N1,最大帧长 32 字节
// ============================================================

uint32_t snid;
void app();
void ASR_CODE();

// 记录当前播报的 ID (用于 done 回调)
static volatile uint8_t g_playing_id = 0;

// 播报完成回调 → 发送 done=<id>\r\n
static void on_play_done(uint16_t play_id) {
  uint8_t pid = g_playing_id;
  if (pid > 0) {
    char buf[16];
    snprintf(buf, sizeof(buf), "done=%u\r\n", pid);
    Serial1.print(buf);
    g_playing_id = 0;
  }
}

// 剥掉尾部空白/CR/LF
static void trim_frame(String &s) {
  while (s.length() > 0) {
    char c = s.charAt(s.length() - 1);
    if (c == '\r' || c == '\n' || c == ' ' || c == '\t')
      s.remove(s.length() - 1);
    else
      break;
  }
}

// 解析 "prefix<dec>",返回数值 (0-255) 或 -1
static int parse_num(const String &s, const char *prefix) {
  int plen = strlen(prefix);
  if (!s.startsWith(prefix)) return -1;
  if ((int)s.length() <= plen) return -1;
  int val = 0;
  for (int i = plen; i < (int)s.length(); i++) {
    char c = s.charAt(i);
    if (c < '0' || c > '9') return -1;
    val = val * 10 + (c - '0');
    if (val > 255) return -1;
  }
  return val;
}

// UART 接收任务 - 按 \n 精确分帧
void app(){
  while (1) {
    if (Serial1.available() > 0) {
      String frame = Serial1.readStringUntil('\n');
      trim_frame(frame);

      // 空帧 / 超长丢弃
      if (frame.length() == 0 || frame.length() > 30) {
        continue;
      }

      // ---- play=<id> ----  id 范围 17-41
      int pid = parse_num(frame, "play=");
      if (pid >= 0) {
        if (pid < 17 || pid > 41) continue;  // 越界丢弃
        g_playing_id = (uint8_t)pid;
        // playid 直接就是协议 ID (编程模式录音时按此表配置)
        prompt_play_by_voice_id((uint16_t)pid, on_play_done, true);
        continue;
      }

      // ---- stop ----
      if (frame == "stop") {
        g_playing_id = 0;
        prompt_stop_play();
        continue;
      }

      // ---- ping ----
      if (frame == "ping") {
        // 协议未定义响应,静默
        continue;
      }

      // 未识别 - 静默忽略
    }
    delay(2);
  }
  vTaskDelete(NULL);
}

void sys_sleep_hook() {
}

// ============================================================
// ASR_CODE - 识别成功钩子
// snid = 编程模式给命令词填的 ID
//   snid==0  → 唤醒词,发 wake\r\n
//   1-16     → 命令词,发 cmd=<snid>\r\n
// ============================================================
void ASR_CODE(){
  set_state_enter_wakeup(15000);  // 15秒无指令进入休眠

  if (snid == 0) {
    Serial1.print("wake\r\n");
  } else if (snid >= 1 && snid <= 16) {
    char buf[16];
    snprintf(buf, sizeof(buf), "cmd=%u\r\n", (unsigned)snid);
    Serial1.print(buf);
  }
  // 越界的 snid 静默丢弃
}

void SystemLed(){
  while (1) {
    digitalWrite(4,0);
    delay(500);
    digitalWrite(4,1);
    delay(500);
  }
  vTaskDelete(NULL);
}

void hardware_init(){
  vol_set(5);
  setPinFun(2,FORTH_FUNCTION);
  setPinFun(3,FORTH_FUNCTION);
  Serial1.begin(9600);
  Serial1.flush();
  xTaskCreate(SystemLed,"SystemLed",128,NULL,3,NULL);
  xTaskCreate(app,"app",512,NULL,4,NULL);
  vTaskDelete(NULL);
}

void setup()
{
  //{ID:0,keyword:"唤醒词",ASR:"小白小白",ASRTO:"我在"}
  //{ID:1,keyword:"命令词",ASR:"关机",ASRTO:""}
  //{ID:2,keyword:"命令词",ASR:"进入动力模式",ASRTO:""}
  //{ID:3,keyword:"命令词",ASR:"进入感应模式",ASRTO:""}
  //{ID:4,keyword:"命令词",ASR:"进入遥控模式",ASRTO:""}
  //{ID:5,keyword:"命令词",ASR:"进入语音模式",ASRTO:""}
  //{ID:6,keyword:"命令词",ASR:"前进",ASRTO:""}
  //{ID:7,keyword:"命令词",ASR:"后退",ASRTO:""}
  //{ID:8,keyword:"命令词",ASR:"左转",ASRTO:""}
  //{ID:9,keyword:"命令词",ASR:"右转",ASRTO:""}
  //{ID:10,keyword:"命令词",ASR:"停止",ASRTO:""}
  //{ID:11,keyword:"命令词",ASR:"左电机正转",ASRTO:""}
  //{ID:12,keyword:"命令词",ASR:"左电机反转",ASRTO:""}
  //{ID:13,keyword:"命令词",ASR:"左电机停止",ASRTO:""}
  //{ID:14,keyword:"命令词",ASR:"右电机正转",ASRTO:""}
  //{ID:15,keyword:"命令词",ASR:"右电机反转",ASRTO:""}
  //{ID:16,keyword:"命令词",ASR:"右电机停止",ASRTO:""}
  //{playid:17,voice:你好呀我是小白进入语音模式}
  //{playid:18,voice:再见啦，小白去休息了}
  //{playid:19,voice:进入动力模式}
  //{playid:20,voice:进入感应模式}
  //{playid:21,voice:进入遥控模式}
  //{playid:22,voice:进入语音模式}
  //{playid:23,voice:遥控已连接}
  //{playid:24,voice:遥控已断开}
  //{playid:25,voice:低电量}
  //{playid:26,voice:收到}
  //{playid:27,voice:前进}
  //{playid:28,voice:后退}
  //{playid:29,voice:左转}
  //{playid:30,voice:右转}
  //{playid:31,voice:停止}
  //{playid:32,voice:靠近启动}
  //{playid:33,voice:遇障停止}
  //{playid:34,voice:挥手开关}
  //{playid:35,voice:明暗调速}
  //{playid:36,voice:左电机正转}
  //{playid:37,voice:左电机反转}
  //{playid:38,voice:左电机停止}
  //{playid:39,voice:右电机正转}
  //{playid:40,voice:右电机反转}
  //{playid:41,voice:右电机停止}
  set_state_enter_wakeup(15000);
  setPinFun(20,SECOND_FUNCTION);
  pinMode(20,output);
  dpmu_set_adio_reuse(PC4,DIGITAL_MODE);
  setPinFun(4,FIRST_FUNCTION);
  pinMode(4,output);
  digitalWrite(20,0);
}
