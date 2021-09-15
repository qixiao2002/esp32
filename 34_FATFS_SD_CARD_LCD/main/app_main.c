
#include "easyio.h"

#define LED 33
#define KEY 0

// 任务句柄，包含创建任务的所有状态，对任务的操作都通过操作任务句柄实现
TaskHandle_t led_task_Handler = NULL;

// led_task 任务，控制LED闪烁
void led_task(void* arg)
{
    // 配置LED为推挽输出，设置初始电平为0
    led_init(LED, 0);
    while(1) {
        // LED状态闪烁
        led_blink(LED);
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

static const char *TAG = "sd";

// spi_lcd_sd_card_fatfs_task 任务。初始化 SPI总线、LCD、SD卡、FATFS文件系统，创建、重命名、写入、读取文件
void spi_lcd_sd_card_fatfs_task(void* arg)
{
    // 配置SPI3-主机模式，配置DMA通道、DMA字节大小，及 MISO、MOSI、CLK的引脚。
    spi_master_init(VSPI_HOST, LCD_DEF_DMA_CHAN, LCD_DMA_MAX_SIZE, SPI3_DEF_PIN_NUM_MISO, SPI3_DEF_PIN_NUM_MOSI, SPI3_DEF_PIN_NUM_CLK);
    // lcd-驱动IC初始化（注意：普通GPIO最大只能30MHz，而IOMUX默认的SPI引脚，CLK最大可以设置到80MHz）（注意排线不要太长，高速时可能会花屏）
    spi_lcd_init(VSPI_HOST, 40*1000*1000, LCD_SPI3_DEF_PIN_NUM_CS0);

    // 清屏，用单一底色
    //LCD_Clear(WHITE);
    //LCD_Clear(LIGHTGRAY);
    LCD_Clear(LGRAYBLUE);


    // SD卡初始化、FATFS文件系统挂载。总线使用SPI模式，20MHz。
    sdmmc_card_t* card = sd_card_fatfs_spi_init();
    while (!card) { // 验证错误，如果返回值为0，则SD卡初始化及FATFS挂载失败，重试
        ESP_LOGE(TAG, "Failed !! %d Retry!!", false);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        card = sd_card_fatfs_spi_init();
    }

    // 使用POSIX和C标准库函数来处理文件。
    // 首先创建一个文件。
    ESP_LOGI(TAG, "Opening file");
    FILE* f = fopen(MOUNT_POINT"/hello.txt", "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    // 将字符串写入到文件中
    fprintf(f, "Hello %s!\n", card->cid.name);
    // 关闭文件
    fclose(f);
    ESP_LOGI(TAG, "File written");

    // 重命名前检查目标文件是否存在
    struct stat st;
    if (stat(MOUNT_POINT"/foo.txt", &st) == 0) {
        // 删除（如果存在）
        unlink(MOUNT_POINT"/foo.txt");
    }

    // 重命名原始文件
    ESP_LOGI(TAG, "Renaming file");
    if (rename(MOUNT_POINT"/hello.txt", MOUNT_POINT"/foo.txt") != 0) {
        ESP_LOGE(TAG, "Rename failed");
        return;
    }

    // 打开重命名的文件以供阅读
    ESP_LOGI(TAG, "Reading file");
    f = fopen(MOUNT_POINT"/foo.txt", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return;
    }
    // 读文件
    char line[64];
    fgets(line, sizeof(line), f);
    fclose(f);
    // 末尾换行符后，插入\0，表示字符串结束
    char* pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);


    // 通过LCD显示SD卡文档的内容
    LCD_ShowString(80-1,20-1,YELLOW,BLUE,line,12,0);

    while(1) {
		vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    // 创建 led_task 任务，任务栈空间大小为 2048，任务优先级为3（configMINIMAL_STACK_SIZE 不能用 printf，会导致ESP32反复重启）
    xTaskCreate(led_task, "led_task", 2048, NULL, 3, &led_task_Handler);

    // 创建 spi_lcd_sd_card_fatfs_task 任务。
    xTaskCreate(spi_lcd_sd_card_fatfs_task, "spi_lcd_sd_card_fatfs_task", 4096, NULL, 3, NULL);
}
