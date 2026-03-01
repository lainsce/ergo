        if (str_eq_c(imp->name, "cogito")) {
            // Try multiple locations for cogito.yi
            char *p = NULL;

            // 1) YIS_COGITO_STDLIB override:
            //    - directory containing cogito.yi, or
            //    - direct path to cogito.yi
            const char *cogito_stdlib = getenv("YIS_COGITO_STDLIB");
            if (cogito_stdlib && cogito_stdlib[0]) {
                if (path_is_file(cogito_stdlib)) {
                    p = strdup(cogito_stdlib);
                    if (!p) {
                        set_err(err, abs_path, "out of memory");
                        return NULL;
                    }
                    if (!load_file(p, root_dir, stdlib_dir, arena, visited, hash, err)) {
                        free(p);
                        return NULL;
                    }
                    free(p);
                    goto found_cogito;
                }
                p = path_join(cogito_stdlib, "cogito.yi");
                if (p && path_is_file(p)) {
                    if (!load_file(p, root_dir, stdlib_dir, arena, visited, hash, err)) {
                        free(p);
                        return NULL;
                    }
                    free(p);
                    goto found_cogito;
                }
                free(p);
                p = NULL;
            }

            // 2) cwd-relative development paths (prefer local checkout)
            const char *cogito_paths[] = {
                "cogito/cogito.yi",
                "cogito/src/cogito.yi",        // split-repo layout: Cogito/src/
                "cogito/_build/cogito.yi",
                "cogito/build/cogito.yi",
                "../cogito/cogito.yi",
                "../cogito/src/cogito.yi",      // split-repo layout: Cogito/src/
                "../cogito/_build/cogito.yi",
                "../cogito/build/cogito.yi",
                "../../cogito/cogito.yi",
                "../../cogito/src/cogito.yi",   // split-repo layout: Cogito/src/
                "../../cogito/_build/cogito.yi",
                "../../cogito/build/cogito.yi",
            };
            for (size_t i = 0; i < sizeof(cogito_paths) / sizeof(cogito_paths[0]); i++) {
                p = strdup(cogito_paths[i]);
                if (p && path_is_file(p)) {
                    if (!load_file(p, root_dir, stdlib_dir, arena, visited, hash, err)) {
                        free(p);
                        return NULL;
                    }
                    free(p);
                    goto found_cogito;
                }
                free(p);
                p = NULL;
            }

            // 3) Installed stdlib locations
            const char *installed_paths[] = {
                "/opt/homebrew/share/yis/stdlib/cogito.yi",
                "/usr/local/share/yis/stdlib/cogito.yi",
                "/usr/share/yis/stdlib/cogito.yi",
            };
            for (size_t i = 0; i < sizeof(installed_paths) / sizeof(installed_paths[0]); i++) {
                p = strdup(installed_paths[i]);
                if (p && path_is_file(p)) {
                    if (!load_file(p, root_dir, stdlib_dir, arena, visited, hash, err)) {
                        free(p);
                        return NULL;
                    }
                    free(p);
                    goto found_cogito;
                }
                free(p);
                p = NULL;
            }

            // 4) stdlib directory configured for this project
            p = path_join(stdlib_dir, "cogito.yi");
            if (p && path_is_file(p)) {
                if (!load_file(p, root_dir, stdlib_dir, arena, visited, hash, err)) {
                    free(p);
                    return NULL;
                }
                free(p);
                goto found_cogito;
            }
            free(p);
            p = NULL;

            // 5) Try relative to the executable
            //    Binary is at <project>/yis/build/yis, so cogito.yi
            //    lives at <exe_dir>/../../cogito/{cogito.yi,_build/cogito.yi,build/cogito.yi}
            {
                char *exe_dir = yis_exe_dir();
                if (exe_dir) {
                    const char *exe_rel[] = {
                        "../../cogito/cogito.yi",
                        "../../cogito/src/cogito.yi",      // split-repo layout
                        "../../cogito/_build/cogito.yi",
                        "../../cogito/build/cogito.yi",
                    };
                    for (size_t j = 0; j < sizeof(exe_rel) / sizeof(exe_rel[0]); j++) {
                        p = path_join(exe_dir, exe_rel[j]);
                        if (p && path_is_file(p)) {
                            if (!load_file(p, root_dir, stdlib_dir, arena, visited, hash, err)) {
                                free(p);
                                free(exe_dir);
                                return NULL;
                            }
                            free(p);
                            free(exe_dir);
                            goto found_cogito;
                        }
                        free(p);
                        p = NULL;
                    }
                    free(exe_dir);
                }
            }
            set_err(err, abs_path, "Cogito GUI framework not found. Install/build Cogito and ensure cogito.yi is discoverable (YIS_COGITO_STDLIB, stdlib, or cogito/build).");
            return NULL;
        found_cogito:
            continue;
        }
