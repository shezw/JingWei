---
name: create-file
description: Guide for creating a new file in the system. Use this when asked to create a new file.
---

To create a new file, follow these steps:

1. When createing a new file for C/C++ projects (.c .cpp .cc .h .hpp), use the following template for the file content. Make sure to replace the placeholders with the appropriate information for your project and file.

```c
/**
    -----------------------------------------------------------

 	{project} JingWei
 	{module} {file}    {Date}
 	
 	@link    : https://github.com/shezw/jingwei
 	@author	 : shezw
 	@email	 : hello@shezw.com

    -----------------------------------------------------------
*/
```

2. if the file is a header file of C, add cpp extern block to make it compatible with C++ projects. The template for the header file content should look like this:

```c
#ifdef __cplusplus
extern "C" {
#endif



#ifdef __cplusplus
}
#endif
```
