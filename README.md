# nvidiagpu
Nvidia GPU Mornitoring and Optimaizing Application .
✨ ฟีเจอร์หลัก
-------------

### 🖥️ **Cross-Platform Support**

-   รองรับ Windows และ macOS
-   ใช้ GLFW + OpenGL สำหรับ GUI
-   Dear ImGui สำหรับ UI ที่ทันสมัย

### 📊 **GPU Monitoring**

-   **Real-time monitoring**: อุณหภูมิ, การใช้งาน GPU/Memory, Power consumption
-   **Live graphs**: กราฟแสดงประสิทธิภาพแบบ real-time
-   **Multiple GPU support**: รองรับหลาย GPU พร้อมกัน
-   **Detailed metrics**: Core clock, Memory clock, Fan speed

### ⚙️ **GPU Tuning Features**

-   **Clock adjustment**: ปรับ Core และ Memory clock
-   **Power limit control**: ควบคุมขีดจำกัดการใช้ไฟ
-   **Custom fan curves**: กำหนด Fan curve ตามอุณหภูมิ
-   **Safety warnings**: คำเตือนความปลอดภัย

### 🎨 **Modern UI Design**

-   **Dark theme**: ธีมสีเข้มที่ทันสมัย
-   **Responsive layout**: การจัดวาง UI แบบ responsive
-   **Docking system**: หน้าต่างสามารถ dock ได้
-   **Live status indicators**: แสดงสถานะแบบ real-time

### 🔧 **Technical Features**

-   **NVML integration**: ใช้ NVIDIA Management Library
-   **Memory-safe**: การจัดการหน่วยความจำที่ปลอดภัย
-   **Multi-threaded**: อัปเดตข้อมูลแบบ background
-   **Profile system**: บันทึกและโหลดโปรไฟล์การตั้งค่า

  ""
  # ติดตั้ง dependencies

vcpkg install glfw3 gl3w imgui nvml

# Build

mkdir build && cd build

cmake ..

make

""
