struct console_driver {
    void (*write)(void *d, const char *s, bytes count);
    void (*config)(void *d, tuple r);
    char *name;
    boolean disabled;
};

typedef closure_type(console_attach, void, struct console_driver *);

void init_console(kernel_heaps kh);
void config_console(tuple root);
