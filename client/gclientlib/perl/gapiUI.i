/* File : gapiUI.i */
%module gapiUI
%include "std_string.i"

%{
#include "../gapiUI.h"
%}

/* Let's just grab the original header file here */
%include "../gapiUI.h"
