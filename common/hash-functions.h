#ifndef _CHATS_COMMON_HASH_FUNCTIONS_H_
# define _CHATS_COMMON_HASH_FUNCTIONS_H_

# include <stddef.h>
# include <stdbool.h>

long long int pearson_hash(const void *data, size_t len);
long long int pearson_hash_update(long long int hash,
                                  const void *data, size_t len);

#endif /* _CHATS_COMMON_HASH_FUNCTIONS_H_ */
