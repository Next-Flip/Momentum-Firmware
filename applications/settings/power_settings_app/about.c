// Since settings app is external, it cannot access firmware functions
// For simple utils like this, easier to include C code in app too
// Instead of copying the file, can (ab)use preprocessor to copy source code here
#include <applications/settings/about/about.c>
// Then, we still use the Header from original code as if nothing happened
