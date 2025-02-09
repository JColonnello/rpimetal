FROM debian AS base
RUN apt update
RUN apt install xz-utils

FROM base AS source
RUN apt install -y curl git
# COPY tarballs/ /tarballs/
WORKDIR /tarballs
RUN git clone --depth 1 --branch gdb-15.2-release git://sourceware.org/git/binutils-gdb.git && \
git clone --depth 1 --branch releases/gcc-14.2.0 git://gcc.gnu.org/git/gcc.git && \
git clone --depth 1 --branch newlib-4.5.0 git://sourceware.org/git/newlib-cygwin.git && \
find -type d -name '.git' | xargs rm -rf
# RUN for f in *.tar*; do tar -xf $f; done
RUN apt install -y bzip2
RUN cd gcc && contrib/download_prerequisites --directory /tarballs
WORKDIR /source

FROM source AS cross
WORKDIR /build
RUN ln -snf /tarballs/binutils-gdb/ binutils
WORKDIR /build/binutils/
RUN for i in gcc fixincludes libatomic libbacktrace libcody libcpp libdecnumber libgcc libsanitizer libstdc++-v3 lto-plugin; do ln -snfv ../gcc/$i; done && \
for i in newlib libgloss; do ln -snfv ../newlib-*/$i; done
# for i in gettext gmp isl mpc mpfr; do ln -snv ../$i-*/ $i; done
WORKDIR /build/obj/
RUN apt update && apt install -y make file texinfo g++ libgmp-dev libmpfr-dev libmpc-dev libisl-dev bison flex libncurses-dev libexpat-dev
ENV target=aarch64-none-elf
RUN ../binutils/configure --prefix=/opt/$target --libexecdir=/opt/$target/lib --target=$target --enable-languages=c,c++ --disable-libstdcxx-pch --with-newlib --with-libgloss --disable-nls --enable-plugins --enable-deterministic-archives --enable-relro --enable-__cxa_atexit --disable-linker-build-id --enable-checking=release --enable-host-shared --disable-libssp --disable-libunwind-exceptions --disable-source-highlight --disable-multilib --disable-bootstrap --enable-tui --with-expat && \
make -j$(nproc) && \
make install && \
cd / && \
rm -rf /build/
ENV PATH="/opt/$target/bin:$PATH" 

FROM cross AS libs
WORKDIR /build/src
RUN apt install -y autogen
RUN find /tarballs/binutils-* -mindepth 1 -maxdepth 1 -type d | xargs -n1 ln -snfv && \
find /tarballs/binutils-* -maxdepth 1 -type f -executable | xargs -n1 ln -snfv && \
for i in /tarballs/binutils-*/Makefile*; do cp $i .; done && \
sed -i -e '/host_modules= { module= sim; }/d' -e '/host_modules= { module= gnulib; }/d' Makefile.def && \
autogen Makefile.def && \
rm gdb readline
WORKDIR /build/obj
RUN LDFLAGS="-Wl,--defsym=_close=0,--defsym=_lseek=0,--defsym=_read=0,--defsym=_write=0,--defsym=_fstat=0,--defsym=_isatty=0,--defsym=_sbrk=0,--defsym=_exit=0,--defsym=_kill=0,--defsym=_getpid=0,--defsym=umask=0,--defsym=chmod=0,--defsym=sysconf=0,--defsym=lstat=0,--defsym=getwd=0,--defsym=_gettimeofday=0,--defsym=_open=0,--defsym=_stat=0,--defsym=_unlink=0,--defsym=access=0,--defsym=_fileno=0,--defsym=dup=0,--defsym=fchmod=0,--defsym=utime=0,--defsym=_setmode=0,--defsym=chdir=0,--defsym=rmdir=0,--defsym=pipe=0,--defsym=sleep=0,--defsym=dup2=0,--defsym=execvp=0,--defsym=execv=0,--defsym=_execve=0,--defsym=_fork=0,--defsym=_wait=0" CFLAGS='-Wno-error=implicit-function-declaration -mtp=tpidr_el1' ../src/configure --prefix= -host=$target --enable-languages=c,c++ --with-newlib --with-libgloss --disable-nls --enable-deterministic-archives --enable-relro --enable-__cxa_atexit --enable-linker-build-id --disable-plugin --disable-libssp -disable-libunwind-exceptions --disable-source-highlight --enable-install-libbfd --without-mmap --disable-largefile --enable-install-libiberty --disable-unit-tests --disable-werror && \
make -j$(nproc) && \
make install DESTDIR=/build/pkg && \
cp -ar /build/pkg/include/ /opt/aarch64-none-elf/aarch64-none-elf/ && \
cp -ar /build/pkg/lib/ /opt/aarch64-none-elf/aarch64-none-elf/ && \
cp -an /build/obj/*/lib*.a /opt/aarch64-none-elf/aarch64-none-elf/lib/ && \
rm -rf /build

FROM base AS toolchain
COPY --from=libs /opt/aarch64-none-elf /opt/aarch64-none-elf
COPY --from=libs /usr/lib/x86_64-linux-gnu/libisl.* /usr/lib/x86_64-linux-gnu/libmpc.* /usr/lib/x86_64-linux-gnu/libmpfr.* /usr/lib/x86_64-linux-gnu/libgmp.* /usr/lib/x86_64-linux-gnu/
RUN apt update && apt install -y make git clangd bear gnupg2 sudo bash-completion qemu-system-aarch64 netcat-openbsd
ARG UNAME=rpi
ARG UID=1000
ARG UPASS=password
RUN groupadd --system ${UNAME} --gid ${UID} && \
useradd --uid ${UID} --system --gid ${UNAME} --home-dir /home/${UNAME} --create-home --comment "Docker image user" --shell /bin/bash ${UNAME} && \
chown -R ${UNAME}:${UNAME} /home/${UNAME} && \
usermod -aG sudo ${UNAME} && \
echo "${UNAME}  ALL=(ALL) ALL" | sudo tee /etc/sudoers.d/permissions && \
echo ${UNAME}:${UPASS} | chpasswd
USER rpi
ENV PATH="/opt/aarch64-none-elf/bin:$PATH" 
WORKDIR /workspaces/rpimetal
RUN mkdir -p "/home/rpi/.config/gdb/" && echo "add-auto-load-safe-path /workspaces/rpimetal/.gdbinit" > "/home/rpi/.config/gdb/gdbinit"