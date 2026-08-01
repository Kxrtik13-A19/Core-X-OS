// =========================================================================================
// COREX OS - MAXIMUM FEATURE KERNEL (Final Release)
// Developer: Kartik Kumar
// Architecture: 32-bit x86 Bare Metal | Custom Window Manager | Omni-Driver Graphics
// =========================================================================================

// -----------------------------------------------------------------------------------------
// SECTION 1: HARDWARE STRUCTURES & DYNAMIC VARIABLES
// -----------------------------------------------------------------------------------------
int W = 800;
int H = 600; 
int pitch = 3200; 
int bpp = 32;

typedef struct {
    unsigned int flags; unsigned int mem_lower; unsigned int mem_upper;
    unsigned int boot_device; unsigned int cmdline; unsigned int mods_count;
    unsigned int mods_addr; unsigned int num; unsigned int size;
    unsigned int addr; unsigned int shndx; unsigned int mmap_length;
    unsigned int mmap_addr; unsigned int drives_length; unsigned int drives_addr;
    unsigned int config_table; unsigned int boot_loader_name; unsigned int apm_table;
    unsigned int vbe_control_info; unsigned int vbe_mode_info;
    unsigned short vbe_mode; unsigned short vbe_interface_seg;
    unsigned short vbe_interface_off; unsigned short vbe_interface_len;
    unsigned long long framebuffer_addr; unsigned int framebuffer_pitch;
    unsigned int framebuffer_width; unsigned int framebuffer_height;
    unsigned char framebuffer_bpp; unsigned char framebuffer_type;
} __attribute__((packed)) multiboot_info_t;

typedef struct {
    unsigned short attributes; unsigned char winA, winB; unsigned short granularity;
    unsigned short winsize; unsigned short segmentA, segmentB; unsigned int realFctPtr;
    unsigned short pitch; unsigned short Xres, Yres;
    unsigned char Wchar, Ychar, planes, bpp, banks;
    unsigned char memory_model, bank_size, image_pages, reserved0;
    unsigned char red_mask, red_position, green_mask, green_position;
    unsigned char blue_mask, blue_position, rsv_mask, rsv_position;
    unsigned char directcolor_attributes; unsigned int physbase;
    unsigned int offscreen_ptr; unsigned short offscreen_size;
} __attribute__((packed)) vbe_mode_info_t;

unsigned char *fb = 0;
unsigned char backbuffer[1024 * 768 * 4]; 

// -----------------------------------------------------------------------------------------
// SECTION 2: LOW-LEVEL PORT DRIVERS & MATH
// -----------------------------------------------------------------------------------------
unsigned char inb(unsigned short p){ 
    unsigned char r; 
    __asm__ volatile ("inb %1, %0" : "=a"(r) : "Nd"(p)); 
    return r; 
}

void outb(unsigned short p, unsigned char d){ 
    __asm__ volatile ("outb %0, %1" : : "a"(d), "Nd"(p)); 
}

unsigned int rand_seed = 12345;
int rand(){ 
    rand_seed = (rand_seed * 1103515245 + 12345) & 0x7FFFFFFF; 
    return rand_seed; 
}

// -----------------------------------------------------------------------------------------
// SECTION 3: CMOS REAL-TIME CLOCK (RTC) DRIVER
// -----------------------------------------------------------------------------------------
unsigned char second, minute, hour, day, month; 
unsigned int year;

int get_update_in_progress_flag() { 
    outb(0x70, 0x0A); 
    return (inb(0x71) & 0x80); 
}

unsigned char get_rtc_register(int reg) { 
    outb(0x70, reg); 
    return inb(0x71); 
}

void read_rtc() {
    while (get_update_in_progress_flag());
    second = get_rtc_register(0x00); 
    minute = get_rtc_register(0x02);
    hour = get_rtc_register(0x04); 
    day = get_rtc_register(0x07);
    month = get_rtc_register(0x08); 
    year = get_rtc_register(0x09);
    
    unsigned char regB = get_rtc_register(0x0B);
    if (!(regB & 0x04)) {
        second = (second & 0x0F) + ((second / 16) * 10); 
        minute = (minute & 0x0F) + ((minute / 16) * 10);
        hour = ( (hour & 0x0F) + (((hour & 0x70) / 16) * 10) ) | (hour & 0x80);
        day = (day & 0x0F) + ((day / 16) * 10); 
        month = (month & 0x0F) + ((month / 16) * 10);
        year = (year & 0x0F) + ((year / 16) * 10);
    }
    if (!(regB & 0x02) && (hour & 0x80)) { hour = ((hour & 0x7F) + 12) % 24; }
    year += (year / 100) * 100 + 2000; 

    // --- HARDCODED DATE OVERRIDE ---
    day = 17;
    month = 5;
    year = 2026;
}

// -----------------------------------------------------------------------------------------
// SECTION 4: HIGH-FIDELITY RENDER ENGINE (Double-Buffered V-Sync)
// -----------------------------------------------------------------------------------------
void px(int x, int y, unsigned int c){
    if(x < 0 || x >= W || y < 0 || y >= H) return;
    unsigned int offset = (y * pitch) + (x * (bpp / 8)); 
    
    if(bpp == 32) {
        *(unsigned int*)(backbuffer + offset) = c;
    } else if (bpp >= 24) { 
        backbuffer[offset] = c & 0xFF; 
        backbuffer[offset+1] = (c >> 8) & 0xFF; 
        backbuffer[offset+2] = (c >> 16) & 0xFF; 
    }
}

void swap_buffers() {
    if(!fb) return;
    for(int y = 0; y < H; y++) {
        for(int x = 0; x < W * (bpp / 8); x++) {
            fb[y * pitch + x] = backbuffer[y * pitch + x];
        }
    }
}

void rect(int x, int y, int w, int h, unsigned int c){ 
    for(int j=y; j<y+h; j++) for(int i=x; i<x+w; i++) px(i, j, c); 
}

void rect_outline(int x, int y, int w, int h, unsigned int c, int t){
    rect(x, y, w, t, c); 
    rect(x, y+h-t, w, t, c); 
    rect(x, y, t, h, c); 
    rect(x+w-t, y, t, h, c); 
}

// -----------------------------------------------------------------------------------------
// SECTION 5: COMPLETE TYPOGRAPHY ENGINE (Standard 5x7 Safe Array)
// -----------------------------------------------------------------------------------------
unsigned char font[96][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x2f,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00}, {0x14,0x7f,0x14,0x7f,0x14},
    {0x24,0x2a,0x7f,0x2a,0x12}, {0x23,0x13,0x08,0x64,0x62}, {0x36,0x49,0x55,0x22,0x50}, {0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1c,0x22,0x41,0x00}, {0x00,0x41,0x22,0x1c,0x00}, {0x14,0x08,0x3e,0x08,0x14}, {0x08,0x08,0x3e,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x60,0x60,0x00,0x00}, {0x20,0x10,0x08,0x04,0x02},
    {0x3e,0x51,0x49,0x45,0x3e}, {0x00,0x42,0x7f,0x40,0x00}, {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4b,0x31},
    {0x18,0x14,0x12,0x7f,0x10}, {0x27,0x45,0x45,0x45,0x39}, {0x3c,0x4a,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1e}, {0x00,0x36,0x36,0x00,0x00}, {0x00,0x56,0x36,0x00,0x00},
    {0x08,0x14,0x22,0x41,0x00}, {0x14,0x14,0x14,0x14,0x14}, {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3e}, {0x7e,0x11,0x11,0x11,0x7e}, {0x7f,0x49,0x49,0x49,0x36}, {0x3e,0x41,0x41,0x41,0x22},
    {0x7f,0x41,0x41,0x22,0x1c}, {0x7f,0x49,0x49,0x49,0x41}, {0x7f,0x09,0x09,0x09,0x01}, {0x3e,0x41,0x49,0x49,0x7a},
    {0x7f,0x08,0x08,0x08,0x7f}, {0x00,0x41,0x7f,0x41,0x00}, {0x20,0x40,0x41,0x3f,0x01}, {0x7f,0x08,0x14,0x22,0x41},
    {0x7f,0x40,0x40,0x40,0x40}, {0x7f,0x02,0x0c,0x02,0x7f}, {0x7f,0x04,0x08,0x10,0x7f}, {0x3e,0x41,0x41,0x41,0x3e},
    {0x7f,0x09,0x09,0x09,0x06}, {0x3e,0x41,0x51,0x21,0x5e}, {0x7f,0x09,0x19,0x29,0x46}, {0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7f,0x01,0x01}, {0x3f,0x40,0x40,0x40,0x3f}, {0x1f,0x20,0x40,0x20,0x1f}, {0x3f,0x40,0x38,0x40,0x3f},
    {0x63,0x14,0x08,0x14,0x63}, {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}, {0x00,0x7f,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20}, {0x00,0x41,0x41,0x7f,0x00}, {0x04,0x02,0x01,0x02,0x04}, {0x40,0x40,0x40,0x40,0x40},
    {0x00,0x01,0x02,0x04,0x00}, {0x20,0x54,0x54,0x54,0x78}, {0x7f,0x48,0x44,0x44,0x38}, {0x38,0x44,0x44,0x44,0x20},
    {0x38,0x44,0x44,0x48,0x7f}, {0x38,0x54,0x54,0x54,0x18}, {0x08,0x7e,0x09,0x01,0x02}, {0x0c,0x52,0x52,0x52,0x3e},
    {0x7f,0x08,0x04,0x04,0x78}, {0x00,0x44,0x7d,0x40,0x00}, {0x20,0x40,0x44,0x3d,0x00}, {0x7f,0x10,0x28,0x44,0x00},
    {0x00,0x41,0x7f,0x40,0x00}, {0x7c,0x04,0x18,0x04,0x78}, {0x7c,0x08,0x04,0x04,0x78}, {0x38,0x44,0x44,0x44,0x38},
    {0x7c,0x14,0x14,0x14,0x08}, {0x08,0x14,0x14,0x18,0x7c}, {0x7c,0x08,0x04,0x04,0x08}, {0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3f,0x44,0x40,0x20}, {0x3c,0x40,0x40,0x20,0x7c}, {0x1c,0x20,0x40,0x20,0x1c}, {0x3c,0x40,0x30,0x40,0x3c},
    {0x44,0x28,0x10,0x28,0x44}, {0x0c,0x50,0x50,0x50,0x3c}, {0x44,0x64,0x54,0x4c,0x44}, {0x00,0x08,0x36,0x41,0x00},
    {0x00,0x00,0x7f,0x00,0x00}, {0x00,0x41,0x36,0x08,0x00}, {0x10,0x08,0x08,0x10,0x08}, {0x00,0x00,0x00,0x00,0x00}
};

void draw_char(int x, int y, char c, unsigned int color, int scale){
    if(c < 32 || c > 127) return; 
    unsigned char *glyph = font[c - 32];
    for(int col = 0; col < 5; col++){
        for(int row = 0; row < 7; row++){
            if(glyph[col] & (1 << row)){
                rect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

void draw_text(int x, int y, const char* str, unsigned int color, int scale){
    int start_x = x;
    while(*str){
        if(*str == '\n') { x = start_x; y += 9 * scale; str++; continue; }
        draw_char(x, y, *str, color, scale); 
        x += 6 * scale; str++;
    }
}

void itoa_pad(int val, char* buf) { 
    buf[0] = '0' + (val / 10); buf[1] = '0' + (val % 10); buf[2] = 0; 
}

// -----------------------------------------------------------------------------------------
// SECTION 6: OS STATE & USER AUTHENTICATION
// -----------------------------------------------------------------------------------------
int os_state = 0; int needs_render = 1; int tick = 0; int welcome_sec_start = 0;
int mouse_x = 400, mouse_y = 300; int mouse_left_click = 0, mouse_prev_click = 0; 
int mouse_byte_num = 0; char mouse_bytes[3];
int is_shift = 0; int is_caps = 0;

char keymap_us[128] = { 0,27,'1','2','3','4','5','6','7','8','9','0','-','=',8,'\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' ' };
char keymap_shift[128] = { 0,27,'!','@','#','$','%','^','&','*','(',')','_','+',8,'\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,'A','S','D','F','G','H','J','K','L',':','"','~',0,'|','Z','X','C','V','B','N','M','<','>','?',0,'*',0,' ' };

char password_buffer[32]; int pass_len = 0; 
const char* CORRECT_PASSWORD = "kartik130124"; 
int unlock_failed = 0;

// -----------------------------------------------------------------------------------------
// SECTION 7: COREX WINDOW MANAGER (Z-Index Compositor)
// -----------------------------------------------------------------------------------------
typedef struct { int id; int active; int visible; int z_index; int x; int y; int w; int h; char title[20]; } Window; 
Window win[4];
int active_app = 1; int dragging_win = -1; int drag_offset_x = 0; int drag_offset_y = 0;

void bring_to_front(int id) {
    int old_z = win[id].z_index;
    for(int i=0; i<4; i++) if(win[i].z_index > old_z) win[i].z_index--;
    win[id].z_index = 3; active_app = id;
}

int point_in_rect(int px, int py, int rx, int ry, int rw, int rh){ return (px >= rx && px <= rx + rw && py >= ry && py <= ry + rh); }

void init_windows(){
    win[0] = (Window){0, 1, 1, 3, 100, 100, 450, 300, "NOTES"}; 
    win[1] = (Window){1, 0, 1, 2, 150, 150, 300, 180, "CALCULATOR"};
    win[2] = (Window){2, 0, 1, 1, 200, 200, 300, 240, "SNAKE GAME"}; 
    win[3] = (Window){3, 0, 1, 0, 250, 250, 410, 350, "DRAWING APP"};
}

// -----------------------------------------------------------------------------------------
// SECTION 8: DESKTOP APPLICATIONS LOGIC
// -----------------------------------------------------------------------------------------
char typing_buffer[800]; int typing_len = 0; char calc_buf[32]; int ci = 0; int calc_result = 0;
int eval(){
    int a=0, b=0, i=0; char op=0;
    while(calc_buf[i]>='0' && calc_buf[i]<='9') { a = a*10 + (calc_buf[i++]-'0'); }
    op = calc_buf[i++];
    while(calc_buf[i]>='0' && calc_buf[i]<='9') { b = b*10 + (calc_buf[i++]-'0'); }
    if(op=='+') return a+b; if(op=='-') return a-b; if(op=='*') return a*b; if(op=='/' || op=='÷') { if(b!=0) return a/b; } return 0;
}

int s_x[100], s_y[100]; int s_len = 3, s_dir = 1; int f_x = 10, f_y = 10; int snake_timer = 0;
void init_snake() { s_len = 3; s_dir = 1; s_x[0] = 5; s_y[0] = 5; s_x[1] = 4; s_y[1] = 5; s_x[2] = 3; s_y[2] = 5; f_x = (rand() % 28) + 1; f_y = (rand() % 18) + 1; }
void update_snake() {
    for(int i = s_len-1; i > 0; i--) { s_x[i] = s_x[i-1]; s_y[i] = s_y[i-1]; }
    if(s_dir == 0) s_y[0]--; if(s_dir == 1) s_x[0]++; if(s_dir == 2) s_y[0]++; if(s_dir == 3) s_x[0]--;
    if(s_x[0] == f_x && s_y[0] == f_y) { if(s_len < 99) s_len++; f_x = (rand() % 28) + 1; f_y = (rand() % 18) + 1; }
    if(s_x[0] < 0 || s_x[0] >= 30 || s_y[0] < 0 || s_y[0] >= 20) init_snake(); 
}

unsigned char canvas_mem[300][400]; 

// -----------------------------------------------------------------------------------------
// SECTION 9: HARDWARE I/O SYNCHRONIZATION (PS/2 Mouse)
// -----------------------------------------------------------------------------------------
void mouse_wait(unsigned char type) {
    int timeout = 100000;
    if(type == 0) { while(timeout--) if((inb(0x64) & 1) == 1) return; }
    else { while(timeout--) if((inb(0x64) & 2) == 0) return; }
}

void mouse_write(unsigned char write) { mouse_wait(1); outb(0x64, 0xD4); mouse_wait(1); outb(0x60, write); }
unsigned char mouse_read() { mouse_wait(0); return inb(0x60); }

void init_mouse() {
    mouse_wait(1); outb(0x64, 0xA8); mouse_wait(1); outb(0x64, 0x20); 
    unsigned char status = mouse_read() | 2;
    mouse_wait(1); outb(0x64, 0x60); mouse_wait(1); outb(0x60, status);
    mouse_write(0xF6); mouse_read(); mouse_write(0xF4); mouse_read(); 
}

// -----------------------------------------------------------------------------------------
// SECTION 10: USER INTERFACE RENDERING
// -----------------------------------------------------------------------------------------
void draw_window(Window *w) {
    if(!w->visible) return;
    rect(w->x + 5, w->y + 5, w->w, w->h, 0x222222); 
    rect(w->x, w->y, w->w, w->h, 0xE0E0E0); 
    rect_outline(w->x, w->y, w->w, w->h, 0x333333, 2); 
    unsigned int header_color = (active_app == w->id) ? 0x0055AA : 0x888888;
    rect(w->x, w->y, w->w, 25, header_color); 
    draw_text(w->x + 10, w->y + 6, w->title, 0xFFFFFF, 2);
    rect(w->x + w->w - 30, w->y + 2, 28, 21, 0xCC0000); 
    draw_text(w->x + w->w - 20, w->y + 5, "X", 0xFFFFFF, 2);
    rect(w->x + 5, w->y + 30, w->w - 10, w->h - 35, 0xFFFFFF); 
    rect_outline(w->x + 5, w->y + 30, w->w - 10, w->h - 35, 0xAAAAAA, 1);

    if(w->id == 0){ 
        int tx = w->x + 15, ty = w->y + 40;
        for(int i = 0; i < typing_len; i++){ 
            draw_char(tx, ty, typing_buffer[i], 0x000000, 2); tx += 14; 
            if(tx > w->x + w->w - 25){ tx = w->x + 15; ty += 20; } 
        }
        if((tick / 150000) % 2 == 0) rect(tx, ty, 2, 15, 0x000000); 
    }
    else if(w->id == 1){ 
        draw_text(w->x + 15, w->y + 50, "EQUATION:", 0x0055AA, 2); 
        rect(w->x + 15, w->y + 70, w->w - 30, 30, 0xDDDDDD);
        for(int i = 0; i < ci; i++) draw_char(w->x + 25 + (i*16), w->y + 78, calc_buf[i], 0x000000, 2);
        draw_text(w->x + 15, w->y + 115, "RESULT:", 0x0055AA, 2);
        int r = calc_result, pos = w->w - 35;
        if (r == 0) draw_char(w->x + pos, w->y + 115, '0', 0x00AA00, 2);
        else { while(r > 0){ draw_char(w->x + pos, w->y + 115, '0' + (r%10), 0x00AA00, 2); r /= 10; pos -= 16; } }
    }
    else if(w->id == 2){ 
        int cx = w->x + 10, cy = w->y + 35; rect(cx, cy, 280, 180, 0x111111); rect(cx + (f_x*9), cy + (f_y*9), 9, 9, 0xFF0000); 
        for(int i=0; i<s_len; i++) rect(cx + (s_x[i]*9), cy + (s_y[i]*9), 9, 9, 0x00FF00); 
    }
    else if(w->id == 3){ 
        int cx = w->x + 5, cy = w->y + 30;
        for(int yy=0; yy<300; yy++){ for(int xx=0; xx<390; xx++){ if(canvas_mem[yy][xx]) px(cx + xx, cy + yy, 0x0055AA); } }
    }
}

void render_desktop(){
    rect(0, 0, W, H, 0x6699CC); rect(0, 0, W, 25, 0x000000); rect(0, 25, W, 2, 0x333333); 
    draw_text(15, 6, "KARTIK KUMAR - COREX OS", 0xFFFFFF, 2);
    char time_str[9] = "00:00:00"; 
    itoa_pad(hour, &time_str[0]); time_str[2] = ':'; itoa_pad(minute, &time_str[3]); time_str[5] = ':'; itoa_pad(second, &time_str[6]);
    draw_text(W - 110, 6, time_str, 0xFFFFFF, 2);
    
    rect(15, 50, 45, 45, 0xE0E0E0); rect_outline(15, 50, 45, 45, 0x000000, 1); draw_text(18, 65, "TXT", 0x0055AA, 2);
    rect(15, 120, 45, 45, 0xE0E0E0); rect_outline(15, 120, 45, 45, 0x000000, 1); draw_text(18, 135, "+-", 0x0055AA, 2);
    rect(15, 190, 45, 45, 0xE0E0E0); rect_outline(15, 190, 45, 45, 0x000000, 1); draw_text(18, 205, "SNK", 0x0055AA, 2);
    rect(15, 260, 45, 45, 0xE0E0E0); rect_outline(15, 260, 45, 45, 0x000000, 1); draw_text(18, 275, "DRW", 0x0055AA, 2);
    
    for(int z = 0; z < 4; z++) { for(int i = 0; i < 4; i++) if(win[i].z_index == z) draw_window(&win[i]); }
    rect(0, H - 30, W, 30, 0x222222); rect(0, H - 32, W, 2, 0x444444);
    
    char date_str[11] = "00/00/0000"; 
    itoa_pad(day, &date_str[0]); date_str[2] = '/'; itoa_pad(month, &date_str[3]); date_str[5] = '/';
    date_str[6] = '0' + ((year/1000)%10); date_str[7] = '0' + ((year/100)%10); date_str[8] = '0' + ((year/10)%10); date_str[9] = '0' + (year%10);
    
    draw_text(15, H - 22, date_str, 0xFFFFFF, 2); 
    draw_text(W - 80, H - 22, "CoreX", 0xFFFFFF, 2);
    draw_text((W / 2) - 130, H - 22, "KOS 13 copyright@2026", 0x4477AA, 2);
    
    rect(mouse_x, mouse_y, 2, 14, 0xFFFFFF); rect(mouse_x, mouse_y, 10, 2, 0xFFFFFF); 
    rect(mouse_x+1, mouse_y+1, 2, 12, 0x000000); rect(mouse_x+1, mouse_y+1, 8, 2, 0x000000);
}

void render_lock_screen(){
    rect(0, 0, W, H, 0x6699CC); rect_outline((W/2)-160, (H/2)-110, 320, 220, 0xFFFFFF, 2); 
    draw_text((W/2)-100, (H/2)-70, "COREX OS", 0xFFFFFF, 4);
    if(unlock_failed) draw_text((W/2)-80, (H/2)-10, "ACCESS DENIED", 0xCC0000, 2); 
    else draw_text((W/2)-85, (H/2)-10, "ENTER PASSWORD", 0xFFFFFF, 2);
    rect((W/2)-110, (H/2)+30, 220, 35, 0xFFFFFF); rect_outline((W/2)-110, (H/2)+30, 220, 35, 0x000000, 2);
    for(int i=0; i<pass_len; i++) draw_char((W/2)-100 + (i*15), (H/2)+40, '*', 0x000000, 2);
    if((tick / 150000) % 2 == 0) rect((W/2)-100 + (pass_len*15), (H/2)+40, 2, 15, 0x000000);
}

void render_welcome_screen(){ 
    rect(0, 0, W, H, 0x6699CC); 
    draw_text((W/2)-105, (H/2)-40, "ACCESS GRANTED", 0x00FF00, 2); 
    draw_text((W/2)-220, (H/2)+10, "Hello Kartik! Welcome To CoreX OS!", 0xFFFFFF, 2); 
}

// -----------------------------------------------------------------------------------------
// SECTION 11: GLOBAL EVENT HANDLER (Input & Physics)
// -----------------------------------------------------------------------------------------
void handle_input(){
    unsigned char status = inb(0x64);
    if(status & 1) { 
        unsigned char data = inb(0x60);
        if(status & 0x20) { 
            if(mouse_byte_num == 0 && !(data & 0x08)) return;
            mouse_bytes[mouse_byte_num++] = data;
            if(mouse_byte_num == 3) { 
                mouse_byte_num = 0; mouse_left_click = mouse_bytes[0] & 0x01;
                int dx = (unsigned char)mouse_bytes[1]; int dy = (unsigned char)mouse_bytes[2];
                if(mouse_bytes[0] & 0x10) dx -= 256; if(mouse_bytes[0] & 0x20) dy -= 256; 
                mouse_x += dx; mouse_y -= dy;
                if(mouse_x < 0) mouse_x = 0; if(mouse_x > W-1) mouse_x = W-1; if(mouse_y < 0) mouse_y = 0; if(mouse_y > H-1) mouse_y = H-1;
                needs_render = 1; 
                if(os_state == 2){
                    if(mouse_left_click && !mouse_prev_click){
                        if(point_in_rect(mouse_x, mouse_y, 15, 50, 45, 45)) { win[0].visible = 1; bring_to_front(0); }
                        if(point_in_rect(mouse_x, mouse_y, 15, 120, 45, 45)) { win[1].visible = 1; bring_to_front(1); }
                        if(point_in_rect(mouse_x, mouse_y, 15, 190, 45, 45)) { win[2].visible = 1; bring_to_front(2); init_snake(); }
                        if(point_in_rect(mouse_x, mouse_y, 15, 260, 45, 45)) { win[3].visible = 1; bring_to_front(3); }
                        for(int z = 3; z >= 0; z--){
                            int clicked = 0;
                            for(int i = 0; i < 4; i++){
                                if(win[i].z_index == z && win[i].visible){
                                    if(point_in_rect(mouse_x, mouse_y, win[i].x + win[i].w - 30, win[i].y + 2, 28, 21)){ win[i].visible = 0; clicked = 1; break; }
                                    if(point_in_rect(mouse_x, mouse_y, win[i].x, win[i].y, win[i].w, 25)){
                                        bring_to_front(i); dragging_win = i; drag_offset_x = mouse_x - win[i].x; drag_offset_y = mouse_y - win[i].y; clicked = 1; break;
                                    }
                                    if(point_in_rect(mouse_x, mouse_y, win[i].x, win[i].y, win[i].w, win[i].h)){ bring_to_front(i); clicked = 1; break; }
                                }
                            }
                            if(clicked) break;
                        }
                    }
                    if(mouse_left_click && dragging_win != -1){ win[dragging_win].x = mouse_x - drag_offset_x; win[dragging_win].y = mouse_y - drag_offset_y; } else { dragging_win = -1; }
                    if(mouse_left_click && active_app == 3 && dragging_win == -1 && win[3].visible){
                        int cx = mouse_x - win[3].x - 5; int cy = mouse_y - win[3].y - 30;
                        if(cx >= 0 && cx < 390 && cy >= 0 && cy < 300) { canvas_mem[cy][cx] = 1; if(cx<389) canvas_mem[cy][cx+1] = 1; if(cy<299) canvas_mem[cy+1][cx] = 1; if(cx<389&&cy<299) canvas_mem[cy+1][cx+1] = 1; }
                    }
                    mouse_prev_click = mouse_left_click;
                }
            }
        } 
        else {
            if(data == 0x2A || data == 0x36) { is_shift = 1; return; } 
            if(data == 0xAA || data == 0xB6) { is_shift = 0; return; } 
            if(data == 0x3A) { is_caps = !is_caps; return; } 
            
            if(data == 0xE0) return; 
            if(data & 0x80) return; 
            
            char c = is_shift ? keymap_shift[data] : keymap_us[data]; 
            
            if(data == 0x48) c = 16; 
            if(data == 0x4B) c = 17; 
            if(data == 0x4D) c = 18; 
            if(data == 0x50) c = 19; 
            
            if(!c) return;
            
            if(is_caps && c >= 'a' && c <= 'z') c -= 32; 
            else if(is_caps && c >= 'A' && c <= 'Z') c += 32;
            
            needs_render = 1;
            
            if(os_state == 0){ 
                if(c == '\n'){
                    int match = 1; for(int i=0; i<pass_len; i++) if(password_buffer[i] != CORRECT_PASSWORD[i]) match=0;
                    if(CORRECT_PASSWORD[pass_len] != 0) match=0;
                    if(match){ os_state = 1; welcome_sec_start = second; } else { unlock_failed = 1; pass_len = 0; }
                }
                else if(c == 8){ if(pass_len > 0) pass_len--; } else if(pass_len < 30) password_buffer[pass_len++] = c; 
            }
            else if(os_state == 2){ 
                if(active_app == 0 && win[0].visible){ 
                    if(c == 8) { if(typing_len > 0) typing_len--; } 
                    else if(typing_len < 790) typing_buffer[typing_len++] = c; 
                }
                else if(active_app == 1 && win[1].visible){ 
                    if(c == 8) { if(ci > 0) ci--; } 
                    else if((c>='0' && c<='9') || c=='+' || c=='-' || c=='*' || c=='/' || c=='÷'){ 
                        if(ci < 30) calc_buf[ci++] = c; 
                    }
                    else if(c == '\n' || c == '='){ calc_result = eval(); ci = 0; }
                }
                else if(active_app == 2 && win[2].visible){ 
                    if((c == 'w' || c == 'W' || c == 16) && s_dir != 2) s_dir = 0; 
                    if((c == 'd' || c == 'D' || c == 18) && s_dir != 3) s_dir = 1;
                    if((c == 's' || c == 'S' || c == 19) && s_dir != 0) s_dir = 2; 
                    if((c == 'a' || c == 'A' || c == 17) && s_dir != 1) s_dir = 3;
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------------------
// SECTION 12: KERNEL ENTRY POINT (The Omni-Detector)
// -----------------------------------------------------------------------------------------
void __attribute__((section(".text"))) kernel_main(unsigned int magic, multiboot_info_t* mbd){
    if (magic == 0x2BADB002) {
        if (mbd->flags & (1 << 12)) {
            fb = (unsigned char *)(unsigned int)(mbd->framebuffer_addr & 0xFFFFFFFF);
            W = mbd->framebuffer_width; H = mbd->framebuffer_height;
            pitch = mbd->framebuffer_pitch; bpp = mbd->framebuffer_bpp;
        } 
        else if (mbd->flags & (1 << 11)) {
            vbe_mode_info_t *vbe = (vbe_mode_info_t *)(unsigned int)mbd->vbe_mode_info;
            fb = (unsigned char *)vbe->physbase;
            W = vbe->Xres; H = vbe->Yres;
            pitch = vbe->pitch; bpp = vbe->bpp;
        }
    }
    if (W == 0 || fb == 0) { W = 800; H = 600; pitch = 3200; bpp = 32; fb = (unsigned char*)0xFD000000; }

    mouse_x = W / 2; mouse_y = H / 2;
    init_mouse(); init_windows(); init_snake(); read_rtc();

    while(1){
        handle_input();
        tick++;
        if(tick > 200000){
            tick = 0; needs_render = 1; read_rtc(); 
            if(os_state == 1) { 
                int diff = second - welcome_sec_start; 
                if(diff < 0) diff += 60; 
                if(diff >= 8) os_state = 2; // ENFORCED 8 SECOND TIMER
            }
            snake_timer++; if(os_state == 2 && active_app == 2 && win[2].visible && snake_timer > 1){ update_snake(); snake_timer = 0; }
        }
        if(needs_render){
            if(os_state == 0) render_lock_screen(); else if(os_state == 1) render_welcome_screen(); else render_desktop();
            swap_buffers(); needs_render = 0;
        }
    }
}