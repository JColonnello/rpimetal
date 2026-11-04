# You can also use GitLab as an alternative to Docker Hub:
# image: registry.gitlab.com/kalaksi-containers/tftpd
FROM docker.io/kalaksi/tftpd
CMD set -eu ;\
    exec in.tftpd -L -vvv -u ftp --secure --address "$TFTPD_BIND_ADDRESS" $TFTPD_EXTRA_ARGS /tftpboot/boot