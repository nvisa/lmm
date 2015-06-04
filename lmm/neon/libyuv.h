#ifndef LIBYUV_H
#define LIBYUV_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char uint8;
extern void UYVYToYRow_NEON(const uint8* src_uyvy, uint8* dst_y, int pix);
extern void YUY2ToYRow_NEON(const uint8* src_uyvy, uint8* dst_y, int pix);
extern void UYVYToUVRow_NEON(const uint8* src_uyvy, int stride_uyvy, uint8* dst_u, uint8* dst_v, int pix);
extern void YUY2ToUVRow_NEON(const uint8* src_yuy2, int stride_yuy2, uint8* dst_u, uint8* dst_v, int pix);
extern void ScaleRowDown2_NEON(const uint8* src_ptr, ptrdiff_t src_stride, uint8* dst, int dst_width);
extern void CopyRow_NEON(const uint8* src, uint8* dst, int count);

#ifdef __cplusplus
}
#endif

#endif // LIBYUV_H
