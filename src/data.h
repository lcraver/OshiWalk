#ifndef APP_DATA_H_
#define APP_DATA_H_

// #ifdef __cplusplus
// extern "C" {
// #endif

/******************************************************************************/
/***        include files                                                   ***/
/******************************************************************************/

#include <stdint.h>

/******************************************************************************/
/***        macro definitions                                               ***/
/******************************************************************************/

/******************************************************************************/
/***        type definitions                                                ***/
/******************************************************************************/

typedef struct touch_calibration_t
{
    uint16_t rawX;
    uint16_t rawY;
} touch_calibration_t;

/******************************************************************************/
/***        exported variables                                              ***/
/******************************************************************************/

/******************************************************************************/
/***        exported functions                                              ***/
/******************************************************************************/

void data_init(void);
bool data_read(touch_calibration_t *data);
bool data_write(touch_calibration_t *data);

int  data_read_gif_index(void);   // returns -1 if not saved yet
bool data_write_gif_index(int index);

typedef struct {
    uint8_t bgBase;
    uint8_t bgBaseColor;
    uint8_t bgShape;
    uint8_t bgShapeColor;

    uint8_t bodyShape;
    uint8_t bodyShapeColor;
    uint8_t bodyClothing;
    uint8_t bodyClothingColor;

    uint8_t hairStyle;      // 0-indexed → Group_(N+1)
    uint8_t hairColor;
    uint8_t bangsStyle;     // 0 = no bangs, N = Group_N

    uint8_t skinColor;      // shared by face base, ears, nose
    uint8_t faceShape;      // 0-indexed → Shape(N+1)
    uint8_t earStyle;       // 0-indexed → Ear(N+1)

    uint8_t eyebrowStyle;   // 0-indexed → Group_(N+1)
    uint8_t eyebrowColor;
    uint8_t eyeStyle;       // 0-indexed → Group_(N+1)
    uint8_t eyeColor;
    uint8_t noseStyle;      // 0-indexed → Group_(N+1)
    uint8_t mouthStyle;     // 0-indexed → Group_(N+1)
    uint8_t mouthColor;

    uint8_t accBodyStyle;
    uint8_t accBodyColor;
    uint8_t accFaceStyle;
    uint8_t accFaceColor;
} avatar_config_t;

bool data_read_avatar(avatar_config_t *cfg);
bool data_write_avatar(const avatar_config_t *cfg);

// #ifdef __cplusplus
// }
// #endif

#endif
/******************************************************************************/
/***        END OF FILE                                                     ***/
/******************************************************************************/