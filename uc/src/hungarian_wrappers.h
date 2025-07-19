#ifndef _HUNGARIAN_WRAPPERS_H

#define TRUE					pdTRUE
#define FALSE					pdFALSE
#define MS_TO_TICKS				pdMS_TO_TICKS
#define MAX_DELAY				portMAX_DELAY
#define delay_ms(ms)				vTaskDelay(ms / portTICK_PERIOD_MS)
#define	create_task				xTaskCreate
#define delete_task				vTaskDelete
#define create_task_pinned_to_core		xTaskCreatePinnedToCore
#define ms_to_ticks				pdMS_TO_TICKS
#define create_queue				xQueueCreate
#define queue_receive				xQueueReceive
#define event_group_create			xEventGroupCreate
#define event_group_delete			vEventGroupDelete
#define event_group_wait_bits			xEventGroupWaitBits
#define event_group_set_bits			xEventGroupSetBits
#define get_current_task_handle			xTaskGetCurrentTaskHandle
#define task_notify_give_indexed_from_isr	vTaskNotifyGiveIndexedFromISR
#define task_notify_take_indexed		ulTaskNotifyTakeIndexed
#define yield_from_isr				portYIELD_FROM_ISR
#define semaphore_create_binary			xSemaphoreCreateBinary
#define semaphore_give				xSemaphoreGive
#define semaphore_take				xSemaphoreTake

#define _HUNGARIAN_WRAPPERS_H
#endif
