#include <linux/notifier.h>

extern int cclogic_get_otg_state(void);

extern int cclogic_register_client(struct notifier_block *nb);
extern int cclogic_unregister_client(struct notifier_block *nb);
extern int cclogic_notifier_call_chain(unsigned long val, void *v);
