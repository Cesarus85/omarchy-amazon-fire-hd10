.syntax unified
.cpu cortex-m4
.thumb
.global hook
hook:
 b.w proxy_hook
