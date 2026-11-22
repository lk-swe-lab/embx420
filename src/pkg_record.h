#ifndef PKG_RECORD_H
#define PKG_RECORD_H
#include "pkg_store.h" /* pkg_record. */
/* Internal validated-record primitives; caller owns records/descriptors. */
void record_free(pkg_record *record);
int record_read(int fd, pkg_record *record, int relaxed, int managed);
int stage_copy(int source, int stage, const pkg_record *record);
int payload_delete(int fd, const pkg_record *record);
int unlink_recorded(int fd, const char *path, int directory);
#endif
