# STM32 Stepper Motor Control System with Multi-UI Interface

Dự án điều khiển động cơ bước (Stepper Motor) sử dụng vi điều khiển **STM32F103C8T6**. Hệ thống cho phép điều chỉnh tốc độ qua biến trở (ADC), thay đổi chế độ quay (Full-step/Half-step), và hiển thị trạng thái hệ thống đồng thời trên LCD 16x2, LED 7 đoạn và UART Terminal.

## 🚀 Tính năng nổi bật
- **Điều khiển động cơ:** Hỗ trợ Full-step và Half-step, đảo chiều quay (CW/CCW).
- **Điều tốc:** Ánh xạ giá trị ADC từ biến trở sang tần số ngắt Timer để thay đổi tốc độ mượt mà.
- **Thư viện tối ưu:** Tự phát triển thư viện **I2C LCD Bit-banging** giúp hiển thị ổn định, chống nháy hình (Flicker-free).
- **Xử lý đa nhiệm:** Quản lý tác vụ không đồng bộ (Asynchronous) giúp hệ thống phản hồi nhanh mà không bị nghẽn (Non-blocking).

## 🛠 Quy trình phát triển

Dự án được triển khai qua 3 giai đoạn chính:

### 1. Cấu hình ngoại vi (STM32CubeMX)
- Thiết lập chân **GPIO** cho nút nhấn (Pull-up) và LED 7 đoạn.
- Cấu hình **ADC1** (Channel 4) ở chế độ lấy mẫu liên tục để đọc biến trở.
- Cấu hình **Timer 2** tạo ngắt định kỳ để quét các bước của động cơ.
- Cấu hình **USART1** để truyền dữ liệu lên máy tính.
- Các chân I2C (PB6, PB7) được để ở chế độ GPIO Output Open-Drain để phục vụ Bit-banging.

### 2. Lập trình logic (KeilC / STM32 HAL Library)
- Xây dựng thuật toán điều khiển động cơ trong hàm ngắt Timer.
- Triển khai thư viện I2C bằng phần mềm để giao tiếp với LCD qua chip PCF8574.
- Tối ưu hóa luồng xử lý: 
  - Cập nhật Motor: Thời gian thực (Timer Interrupt).
  - Cập nhật UI (LCD/LED): 200ms/lần.
  - Truyền thông UART: 2s/lần để giảm tải hệ thống.

### 3. Mô phỏng và Kiểm thử (Proteus)
- Sử dụng Driver **L298** để điều khiển động cơ bước 4 dây (Bipolar).
- Kết nối **Virtual Terminal** để giám sát dữ liệu truyền qua UART.
- Kiểm tra tính đúng đắn của logic điều khiển và khả năng chịu tải của hệ thống mô phỏng.
---
