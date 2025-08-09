#ifndef SWITCH_H
#define SWITCH_H

#ifdef __cplusplus
extern "C" {
#endif

void switch_init();
bool switch_get_state_left(void);
bool switch_get_state_right(void);

#ifdef __cplusplus
}
#endif

#endif // SWITCH_H
