#include <Arduino.h>

#include <M5Unified.h>
#include <esp_camera.h>
#include <fb_gfx.h>

#include <vector>
#include "human_face_detect_msr01.hpp"
#include "human_face_detect_mnp01.hpp"
#define TWO_STAGE 1 /*<! 1: detect by two-stage which is more accurate but slower(with keypoints). */
                    /*<! 0: detect by one-stage which is less accurate but faster(without keypoints). */

#include "face_recognition_tool.hpp"
#include "face_recognition_112_v1_s16.hpp"
#include "face_recognition_112_v1_s8.hpp"
#define QUANT_TYPE 0 //if set to 1 => very large firmware, very slow, reboots when streaming...
#define FACE_ID_SAVE_NUMBER 7


#define LCD_WIDTH (320)
#define LCD_HEIGHT  (240)
#define LCD_BUF_SIZE (LCD_WIDTH*LCD_HEIGHT*2)

static int8_t is_enrolling = 0;
FaceRecognition112V1S8 recognizer;

static camera_config_t camera_config = {
    .pin_pwdn     = -1,
    .pin_reset    = -1,
    .pin_xclk     = 2,
    .pin_sscb_sda = 12,
    .pin_sscb_scl = 11,

    .pin_d7 = 47,
    .pin_d6 = 48,
    .pin_d5 = 16,
    .pin_d4 = 15,
    .pin_d3 = 42,
    .pin_d2 = 41,
    .pin_d1 = 40,
    .pin_d0 = 39,

    .pin_vsync = 46,
    .pin_href  = 38,
    .pin_pclk  = 45,

    .xclk_freq_hz = 20000000,
    .ledc_timer   = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_RGB565,
    //.pixel_format = PIXFORMAT_JPEG,
    .frame_size   = FRAMESIZE_QVGA,   // QVGA(320x240)
    .jpeg_quality = 0,
    .fb_count     = 2,
    .fb_location  = CAMERA_FB_IN_PSRAM,
    .grab_mode    = CAMERA_GRAB_WHEN_EMPTY,
};

esp_err_t camera_init(){

    //initialize the camera
    M5.In_I2C.release();
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        //Serial.println("Camera Init Failed");
        M5.Display.println("Camera Init Failed");
        return err;
    }

    return ESP_OK;
}

void check_heap_free_size(void){
  Serial.printf("===============================================================\n");
  Serial.printf("Mem Test\n");
  Serial.printf("===============================================================\n");
  Serial.printf("esp_get_free_heap_size()                              : %6d\n", esp_get_free_heap_size() );
  Serial.printf("heap_caps_get_free_size(MALLOC_CAP_DMA)               : %6d\n", heap_caps_get_free_size(MALLOC_CAP_DMA) );
  Serial.printf("heap_caps_get_free_size(MALLOC_CAP_SPIRAM)            : %6d\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM) );
  Serial.printf("heap_caps_get_free_size(MALLOC_CAP_INTERNAL)          : %6d\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL) );
  Serial.printf("heap_caps_get_free_size(MALLOC_CAP_DEFAULT)           : %6d\n", heap_caps_get_free_size(MALLOC_CAP_DEFAULT) );

}



#define FACE_COLOR_WHITE  0x00FFFFFF
#define FACE_COLOR_BLACK  0x00000000
#define FACE_COLOR_RED    0x000000FF
#define FACE_COLOR_GREEN  0x0000FF00
#define FACE_COLOR_BLUE   0x00FF0000
#define FACE_COLOR_YELLOW (FACE_COLOR_RED | FACE_COLOR_GREEN)
#define FACE_COLOR_CYAN   (FACE_COLOR_BLUE | FACE_COLOR_GREEN)
#define FACE_COLOR_PURPLE (FACE_COLOR_BLUE | FACE_COLOR_RED)

static void rgb_print(fb_data_t *fb, uint32_t color, const char *str)
{
    fb_gfx_print(fb, (fb->width - (strlen(str) * 14)) / 2, 10, color, str);
}

static int rgb_printf(fb_data_t *fb, uint32_t color, const char *format, ...)
{
    char loc_buf[64];
    char *temp = loc_buf;
    int len;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    len = vsnprintf(loc_buf, sizeof(loc_buf), format, arg);
    va_end(copy);
    if (len >= sizeof(loc_buf))
    {
        temp = (char *)malloc(len + 1);
        if (temp == NULL)
        {
            return 0;
        }
    }
    vsnprintf(temp, len + 1, format, arg);
    va_end(arg);
    rgb_print(fb, color, temp);
    if (len > 64)
    {
        free(temp);
    }
    return len;
}

static void draw_face_boxes(fb_data_t *fb, std::list<dl::detect::result_t> *results, int face_id)
{
    int x, y, w, h;
    uint32_t color = FACE_COLOR_YELLOW;
    if (face_id < 0)
    {
        color = FACE_COLOR_RED;
    }
    else if (face_id > 0)
    {
        color = FACE_COLOR_GREEN;
    }
    if(fb->bytes_per_pixel == 2){
        //color = ((color >> 8) & 0xF800) | ((color >> 3) & 0x07E0) | (color & 0x001F);
        color = ((color >> 16) & 0x001F) | ((color >> 3) & 0x07E0) | ((color << 8) & 0xF800);
    }
    int i = 0;
    for (std::list<dl::detect::result_t>::iterator prediction = results->begin(); prediction != results->end(); prediction++, i++)
    {
        // rectangle box
        x = (int)prediction->box[0];
        y = (int)prediction->box[1];

        // yが負の数のときにfb_gfx_drawFastHLine()でメモリ破壊してリセットする不具合の対策
        if(y < 0){
           y = 0;
        }

        w = (int)prediction->box[2] - x + 1;
        h = (int)prediction->box[3] - y + 1;

        //Serial.printf("x:%d y:%d w:%d h:%d\n", x, y, w, h);

        if((x + w) > fb->width){
            w = fb->width - x;
        }
        if((y + h) > fb->height){
            h = fb->height - y;
        }

        //Serial.printf("x:%d y:%d w:%d h:%d\n", x, y, w, h);

        //fb_gfx_fillRect(fb, x+10, y+10, w-20, h-20, FACE_COLOR_RED);  //モザイク
        fb_gfx_drawFastHLine(fb, x, y, w, color);
        fb_gfx_drawFastHLine(fb, x, y + h - 1, w, color);
        fb_gfx_drawFastVLine(fb, x, y, h, color);
        fb_gfx_drawFastVLine(fb, x + w - 1, y, h, color);

#if TWO_STAGE
        // landmarks (left eye, mouth left, nose, right eye, mouth right)
        int x0, y0, j;
        for (j = 0; j < 10; j+=2) {
            x0 = (int)prediction->keypoint[j];
            y0 = (int)prediction->keypoint[j+1];
            fb_gfx_fillRect(fb, x0, y0, 3, 3, color);
        }
#endif
    }
}

static int run_face_recognition(fb_data_t *fb, fb_data_t *fb_lcd, std::list<dl::detect::result_t> *results)
{
    std::vector<int> landmarks = results->front().keypoint;
    int id = -1;

    Tensor<uint8_t> tensor;
    tensor.set_element((uint8_t *)fb->data).set_shape({fb->height, fb->width, 3}).set_auto_free(false);

    int enrolled_count = recognizer.get_enrolled_id_num();

    if (enrolled_count < FACE_ID_SAVE_NUMBER && is_enrolling){
        id = recognizer.enroll_id(tensor, landmarks, "", true);
        log_i("Enrolled ID: %d", id);
        rgb_printf(fb_lcd, FACE_COLOR_CYAN, "ID[%u]", id);
        is_enrolling = 0;
    }

    face_info_t recognize = recognizer.recognize(tensor, landmarks);
    if(recognize.id >= 0){
        rgb_printf(fb_lcd, FACE_COLOR_GREEN, "ID[%u]: %.2f", recognize.id, recognize.similarity);
    } else {
        rgb_print(fb_lcd, FACE_COLOR_RED, "Intruder Alert!");
    }
    return recognize.id;
}

esp_err_t camera_capture_and_face_recognition(){
  size_t out_len, out_width, out_height;
  uint8_t *out_buf;
  bool s;
  int face_id = 0;  
  
  //acquire a frame
  M5.In_I2C.release();
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    //Serial.println("Camera Capture Failed");
    M5.Display.println("Camera Capture Failed");
    return ESP_FAIL;
  }


  out_len = fb->width * fb->height * 3;
  out_width = fb->width;
  out_height = fb->height;
  out_buf = (uint8_t*)malloc(out_len);
  if (!out_buf) {
    Serial.println("out_buf malloc failed");
    return ESP_FAIL;
  }
  s = fmt2rgb888(fb->buf, fb->len, fb->format, out_buf);
  //esp_camera_fb_return(fb);
  if (!s) {
    free(out_buf);
    Serial.println("To rgb888 failed");
    return ESP_FAIL;
  }

  // 顔検出、顔認識用バッファ(RGB888) ※↑でmallocしたバッファを使用
  fb_data_t rfb;
  rfb.width = out_width;
  rfb.height = out_height;
  rfb.data = out_buf;
  rfb.bytes_per_pixel = 3;
  rfb.format = FB_BGR888;

  // LCD表示用バッファ(RGB565)　※カメラキャプチャのバッファをそのまま使用
  fb_data_t rfbLcd;
  rfbLcd.width = fb->width;
  rfbLcd.height = fb->height;
  rfbLcd.data = fb->buf;
  rfbLcd.bytes_per_pixel = 2;
  rfbLcd.format = FB_RGB565;

#if TWO_STAGE
  HumanFaceDetectMSR01 s1(0.1F, 0.5F, 10, 0.2F);
  HumanFaceDetectMNP01 s2(0.5F, 0.3F, 5);
  std::list<dl::detect::result_t> &candidates = s1.infer((uint8_t *)out_buf, {(int)out_height, (int)out_width, 3});
  std::list<dl::detect::result_t> &results = s2.infer((uint8_t *)out_buf, {(int)out_height, (int)out_width, 3}, candidates);
#else
  HumanFaceDetectMSR01 s1(0.3F, 0.5F, 10, 0.2F);
  std::list<dl::detect::result_t> &results = s1.infer((uint8_t *)out_buf, {(int)out_height, (int)out_width, 3});
#endif

  if (results.size() > 0) {
    face_id = run_face_recognition(&rfb, &rfbLcd, &results);

    draw_face_boxes(&rfbLcd, &results, face_id);
  }

  //replace this with your own function
  //process_image(fb->width, fb->height, fb->format, fb->buf, fb->len);
  M5.Display.startWrite();
  M5.Display.setAddrWindow(0, 0, LCD_WIDTH, LCD_HEIGHT);
  M5.Display.writePixels((uint16_t*)fb->buf, int(fb->len / 2));
  M5.Display.endWrite();
  
  //Serial.println("<heap size before fb return>");  
  //check_heap_free_size();

  //return the frame buffer back to the driver for reuse
  esp_camera_fb_return(fb);
  free(out_buf);

  //Serial.println("<heap size after fb return>");  
  //check_heap_free_size();

  return ESP_OK;
}


void setup() {
  auto cfg = M5.config();
  cfg.output_power = true;
  M5.begin(cfg);

  M5.Display.setFont(&fonts::efontJA_24);
  M5.Display.println("HelloWorld");
  Serial.println("Hello World");

  camera_init();

}

void loop() {
  //check_heap_free_size();

  camera_capture_and_face_recognition();

  M5.update();
  auto t = M5.Touch.getDetail();
  if (t.wasPressed())
  {
    // 顔検出しているときに画面タッチすると、その顔を覚える
    is_enrolling = 1;
    M5.Speaker.tone(1000, 100);
  }

  //delay(1000);          // Delay [ms]

}
