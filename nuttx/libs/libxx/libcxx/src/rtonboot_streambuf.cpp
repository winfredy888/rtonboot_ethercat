#include <iostream>
#include <streambuf>

extern "C" 
{
	extern int up_putc(int ch);
}	

class RtonbootStreambuf : public std::streambuf {
private:
    
protected:
    virtual int_type overflow(int_type c) override {
        if (c != traits_type::eof()) {
            char ch = static_cast<char>(c);
            up_putc(ch);
        }
        return c;
    }
    
    virtual std::streamsize xsputn(const char* s, std::streamsize count) override {
        for (std::streamsize i = 0; i < count; ++i) {
            up_putc(s[i]);
        }
        return count;
    }
};

extern "C" 
{

void setup_libcxx_output(void) {
    static RtonbootStreambuf rtonboot_streambuf;
    
    std::cout.rdbuf(&rtonboot_streambuf);
     
    std::cerr.rdbuf(&rtonboot_streambuf);
    
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
}

}	
