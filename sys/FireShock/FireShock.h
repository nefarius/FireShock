/*
MIT License

Copyright (c) 2017 Benjamin "Nefarius" Höglinger

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#pragma once

#define IOCTL_INDEX             0x801
#define FILE_DEVICE_FIRESHOCK   32768U

#define IOCTL_FIRESHOCK_GET_HOST_BD_ADDR        CTL_CODE(FILE_DEVICE_FIRESHOCK, \
                                                            IOCTL_INDEX + 0x00, \
                                                            METHOD_BUFFERED,    \
                                                            FILE_READ_ACCESS)

#define IOCTL_FIRESHOCK_GET_DEVICE_BD_ADDR      CTL_CODE(FILE_DEVICE_FIRESHOCK, \
                                                            IOCTL_INDEX + 0x01, \
                                                            METHOD_BUFFERED,    \
                                                            FILE_READ_ACCESS)

#define IOCTL_FIRESHOCK_SET_HOST_BD_ADDR        CTL_CODE(FILE_DEVICE_FIRESHOCK, \
                                                            IOCTL_INDEX + 0x02, \
                                                            METHOD_BUFFERED,    \
                                                            FILE_WRITE_ACCESS)

#define SET_HOST_BD_ADDR_CONTROL_BUFFER_LENGTH  8

#include <pshpack1.h>

/**
* \typedef struct _BD_ADDR
*
* \brief   Defines a Bluetooth client MAC address.
*/
typedef struct _BD_ADDR
{
    BYTE Address[6];

} BD_ADDR, *PBD_ADDR;

typedef enum _DS_DEVICE_TYPE
{
    DsTypeUnknown,
    DualShock3,
    DualShock4

} DS_DEVICE_TYPE, *PDS_DEVICE_TYPE;


typedef struct _FIRESHOCK_GET_HOST_BD_ADDR
{
    BD_ADDR Host;

} FIRESHOCK_GET_HOST_BD_ADDR, *PFIRESHOCK_GET_HOST_BD_ADDR;

typedef struct _FIRESHOCK_GET_DEVICE_BD_ADDR
{
    BD_ADDR Device;

} FIRESHOCK_GET_DEVICE_BD_ADDR, *PFIRESHOCK_GET_DEVICE_BD_ADDR;

typedef struct _FIRESHOCK_SET_HOST_BD_ADDR
{
    BD_ADDR Host;

} FIRESHOCK_SET_HOST_BD_ADDR, *PFIRESHOCK_SET_HOST_BD_ADDR;

#include <poppack.h>
