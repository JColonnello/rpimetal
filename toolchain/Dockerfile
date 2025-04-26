FROM debian AS base
RUN rm /bin/sh && ln -s /bin/bash /bin/sh
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt update

FROM base AS source
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt update && apt --no-install-recommends install -y ca-certificates curl git bzip2 cpio xz-utils
WORKDIR /tarballs/root
RUN git clone --depth 1 --branch gdb-15.2-release git://sourceware.org/git/binutils-gdb.git && \
git clone --depth 1 --branch releases/gcc-14.2.0 git://gcc.gnu.org/git/gcc.git && \
git clone --depth 1 --branch newlib-4.5.0 git://sourceware.org/git/newlib-cygwin.git && \
find -type d -name '.git' | xargs rm -rf && \

## Uncomment this section to build gcc dependencies from source
# mkdir -p /tarballs/gcc-deps && \
# cd gcc && \
#     contrib/download_prerequisites --directory /tarballs/gcc-deps && \
# cd /tarballs/gcc-deps && \
#     rm *.tar.* && \
#     ls | perl -ne 'print if s/^((.+)-(?:[0-9]+\.?)+)$/$1 $2/' | xargs -L1 mv && \
#     for d in *; do rm /tarballs/root/gcc/$d; done && \

mkdir -p /usr/local/src && \
cd /tarballs && \
    for d in root/newlib-cygwin root/gcc root/binutils-gdb; do pushd $d; find . -print | cpio -pdlmu /usr/local/src; popd; done
# cd /tarballs/root/binutils-gdb && \
#     find . -print | cpio -pdlmu /usr/local/src
WORKDIR /usr/local/src
COPY syscalls.patch binutils-src.patch .
RUN git apply < syscalls.patch && \
git apply < binutils-src.patch

FROM base AS compile
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt update && apt --no-install-recommends install -y make file texinfo g++ libgmp-dev libmpfr-dev libmpc-dev libisl-dev bison flex libncurses-dev libexpat-dev python3-dev bear
WORKDIR /build/obj/

FROM compile AS cross
ENV target=aarch64-none-elf
RUN --mount=type=bind,from=source,source=/usr/local/src,target=/usr/local/src,rw CFLAGS_FOR_TARGET='-mtp=tpidr_el1 -g -O2' /usr/local/src/configure --prefix=/opt/$target --libexecdir=/opt/$target/lib --target=$target --enable-languages=c,c++ --disable-libstdcxx-pch --with-newlib --with-libgloss --disable-nls --enable-plugins --enable-deterministic-archives --enable-relro --enable-__cxa_atexit --disable-linker-build-id --enable-checking=release --enable-host-shared --disable-libssp --disable-libunwind-exceptions --disable-source-highlight --disable-multilib --disable-bootstrap --enable-tui --with-expat --with-python --enable-newlib-reent-thread-local --enable-newlib-multithread && \
# mkdir -p /opt/aarch64-none-elf/ && bear --append --output /opt/aarch64-none-elf/compile_commands.json -- make -j$[$(nproc) - 1] && \
make -j$[$(nproc) - 1] && \
make install && \
cd / && \
rm -rf /build/
COPY specs /opt/aarch64-none-elf/lib/gcc/aarch64-none-elf/specs
ENV PATH="/opt/$target/bin:$PATH" 

FROM compile AS libs
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
--mount=type=cache,target=/var/lib/apt,sharing=locked \
apt update && apt --no-install-recommends install -y autogen patch
ENV target=aarch64-none-elf
ENV PATH="/opt/$target/bin:$PATH" 
WORKDIR /build/obj
COPY --from=cross /opt/aarch64-none-elf /opt/aarch64-none-elf
COPY binutils-*.patch /
RUN --mount=type=bind,from=source,source=/tarballs/root/binutils-gdb,target=/usr/local/src,rw pushd /usr/local/src && \
    cat /*.patch | patch -p1 && \
    autogen Makefile.def && \
    rm -r gdb readline && \
    popd && \
LDFLAGS="-Wl,--defsym=_close=0,--defsym=_lseek=0,--defsym=_read=0,--defsym=_write=0,--defsym=_fstat=0,--defsym=_isatty=0,--defsym=_sbrk=0,--defsym=_exit=0,--defsym=_kill=0,--defsym=_getpid=0,--defsym=umask=0,--defsym=chmod=0,--defsym=sysconf=0,--defsym=lstat=0,--defsym=getwd=0,--defsym=_gettimeofday=0,--defsym=_open=0,--defsym=_stat=0,--defsym=_unlink=0,--defsym=access=0,--defsym=_fileno=0,--defsym=dup=0,--defsym=fchmod=0,--defsym=utime=0,--defsym=_setmode=0,--defsym=chdir=0,--defsym=rmdir=0,--defsym=pipe=0,--defsym=sleep=0,--defsym=dup2=0,--defsym=execvp=0,--defsym=execv=0,--defsym=_execve=0,--defsym=_fork=0,--defsym=_wait=0" CFLAGS='-Wno-error=implicit-function-declaration -mtp=tpidr_el1 -g -O2' /usr/local/src/configure --prefix= -host=$target --enable-install-libbfd --without-mmap --disable-largefile --enable-install-libiberty --disable-unit-tests --disable-werror --enable-tls --enable-newlib-reent-thread-local --enable-newlib-multithread && \
# bear --append --output /opt/aarch64-none-elf/compile_commands.json -- make -j$[$(nproc) - 1] && \
make -j$[$(nproc) - 1] && \
make install DESTDIR=/build/pkg && \
cp -ar /build/pkg/include/ /opt/aarch64-none-elf/aarch64-none-elf/ && \
cp -ar /build/pkg/lib/ /opt/aarch64-none-elf/aarch64-none-elf/ && \
cp -an /build/obj/*/lib*.a /opt/aarch64-none-elf/aarch64-none-elf/lib/ && \
rm -rf /build

FROM base AS toolchain
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
--mount=type=cache,target=/var/lib/apt,sharing=locked \
apt update && apt install -y make git clangd bear gnupg2 sudo bash-completion qemu-system-aarch64 netcat-openbsd python3 libpython3.11 locales
ARG UNAME=rpi
ARG UID=1000
ARG UPASS=password
RUN groupadd --system ${UNAME} --gid ${UID} && \
useradd --uid ${UID} --system --gid ${UNAME} --home-dir /home/${UNAME} --create-home --comment "Docker image user" --shell /bin/bash ${UNAME} && \
chown -R ${UNAME}:${UNAME} /home/${UNAME} && \
usermod -aG sudo ${UNAME} && \
echo "${UNAME}  ALL=(ALL) ALL" | sudo tee /etc/sudoers.d/permissions && \
echo ${UNAME}:${UPASS} | chpasswd
RUN SNIPPET="export PROMPT_COMMAND='history -a' && export HISTFILE=/commandhistory/.bash_history" \
&& mkdir /commandhistory \
&& touch /commandhistory/.bash_history \
&& chown -R $UNAME /commandhistory \
&& echo "$SNIPPET" >> "/home/$UNAME/.bashrc"
COPY --from=source /usr/local/src /usr/local/src
COPY --from=compile /usr/lib/x86_64-linux-gnu/libisl.* /usr/lib/x86_64-linux-gnu/libmpc.* /usr/lib/x86_64-linux-gnu/libmpfr.* /usr/lib/x86_64-linux-gnu/libgmp.* /usr/lib/x86_64-linux-gnu/
COPY --from=libs /opt/aarch64-none-elf /opt/aarch64-none-elf
# RUN cp -a /opt/aarch64-none-elf/compile_commands.json /usr/local/src/
ENV PATH="/opt/aarch64-none-elf/bin:$PATH" 
# Set the locale
RUN sed -i '/en_US.UTF-8/s/^# //g' /etc/locale.gen && \
locale-gen
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8
USER rpi
WORKDIR /workspaces/rpimetal
RUN mkdir -p "/home/rpi/.config/gdb/" && echo "add-auto-load-safe-path /workspaces/rpimetal/.gdbinit" > "/home/rpi/.config/gdb/gdbinit"