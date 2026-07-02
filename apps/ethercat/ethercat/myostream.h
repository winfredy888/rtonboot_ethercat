#ifndef __MYOSTREAM_H__
#define __MYOSTREAM_H__

#include <stdio.h>

#include <sstream>

extern "C" 
{
	/*	extern int linux_printf(const char *fmt, ...);
	extern void linux_serial_puts(const char *s);
	extern int linux_vprintf(const char *fmt, va_list ap);
	extern void linux_serial_putc(int ch);*/
	extern int pipe_printf(const char *fmt, ...);
	extern void pipe_puts(const char *s);
	extern void pipe_putc(const char c);
	extern int pipe_vprintf(const char *fmt, va_list ap);
}	

class printf_ostream {
private:

	void print(const char * const str){
        pipe_printf("%s", str);
    }
    
    void print(char * str){
        pipe_printf("%s", str);
    }
    
    void print(const std::string& s) 
    {
        pipe_puts(s.c_str());
    }
    
    void print(char c){
        pipe_printf("%c", c);
    }
    
    void print(int i){
        pipe_printf("%d", i);
    }
    
    void print(long l){
        pipe_printf("%l", l);
    }
    
    void print(long unsigned int l){
        pipe_printf("%l", l);
    }
    
    void print(double d){
        pipe_printf("%f", d);  
    }
 
public:
    printf_ostream(){}
 
    ~printf_ostream() {
    }
    
    template<typename T>
    printf_ostream& operator<<(const T& value){
        print(value);
        return *this;
    }
 
    /*printf_ostream& operator<<(int value) {
        linux_printf("%d", value);
        return *this;
    }
 
    printf_ostream& operator<<(double value) {
        linux_printf("%f", value);
        return *this;
    }
 
    printf_ostream& operator<<(const char* str) {
        linux_serial_puts(str);
        return *this;
    }
 
    printf_ostream& operator<<(const std::string& s) {
        linux_serial_puts(s.c_str());
        return *this;
    }*/
};

extern printf_ostream mycsout;

/*class MyOStream {
public:
    MyOStream& setfill(char c) {
        fill_char = c;
        return *this;
    }

    MyOStream& setw(int w) {
        field_width = w;
        return *this;
    }

    template <typename T>
    MyOStream& operator<<(const T& value) {
        linux_printf("%*s", field_width, value);
        return *this;
    }

    MyOStream& operator<<(char c) {
        linux_printf("%*c", field_width, c);
        return *this;
    }

private:
    char fill_char = ' ';
    int field_width = 0;
};*/

class MyOStream {
public:
    MyOStream() : fill_char(' '), width(0) {}

    MyOStream& setfill(char c) {
        fill_char = c;
        
        return *this;
    }

    MyOStream& setw(int w) {
        width = w;
        
        return *this;
    }

    MyOStream& print(const char* format, ...) {
        va_list args;
        va_start(args, format);

        int len = vsnprintf(nullptr, 0, format, args);

        for (int i = 0; i < width - len; ++i) {
            pipe_putc(fill_char);
        }

        va_start(args, format);
        pipe_vprintf(format, args);
        va_end(args);
        
        return *this;
    }

private:
    char fill_char;
    int width;
};

extern MyOStream mycfout;

extern stringstream g_err;

#endif
