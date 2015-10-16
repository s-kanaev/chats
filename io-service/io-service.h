#ifndef _IO_SERVICE_H_
# define _IO_SERVICE_H_

# include "lib.h"

# include <stdbool.h>

/** IO service operation
 * read/write
 */
typedef enum io_svc_op {
    IO_SVC_OP_READ = 0,
    IO_SVC_OP_WRITE = 1,
    IO_SVC_OP_COUNT
} io_svc_op_t;

/** IO service object type forward declaration
 */
struct io_service;
typedef struct io_service io_service_t;

typedef void (*iosvc_job_function_t)(int fd, io_svc_op_t op, void *ctx);

io_service_t *io_service_init();
void io_service_stop(io_service_t *iosvc, bool wait_pending);
void io_service_deinit(io_service_t *iosvc);
void io_service_post_job(io_service_t *iosvc,
                         int fd, io_svc_op_t op,
                         iosvc_job_function_t job, void *ctx);
void io_service_run(io_service_t *iosvc);
void io_service_remove_job(io_service_t *iosvc,
                           int fd, io_svc_op_t op,
                           iosvc_job_function_t job, void *ctx);

#endif /* _IO_SERVICE_H_ */