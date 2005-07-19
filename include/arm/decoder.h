#ifndef __ARM_DECODER_H
#define __ARM_DECODER_H

#include <arm/arm.h>

void arm_decode_into_uop(struct uop *op);
void thumb_decode_into_uop(struct uop *op);

#endif

