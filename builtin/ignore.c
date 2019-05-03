#include "cache.h"
#include "config.h"
#include "builtin.h"
#include "parse-options.h"
#include "string-list.h"

#define SCOPE_SHARE (1 << 0)
#define SCOPE_LOCAL (1 << 1)
#define SCOPE_SYSTEM (1 << 2)

#define ACTION_TRACK (1 << 0)
#define ACTION_UNTRACK (1 << 1)
#define ACTION_EDIT (1 << 2)

static const char * const ignore_usage[] = {
	N_("git ignore [--share|--local|--system] [--track|--untrack] <pattern>..."),
	N_("git ignore [--share|--local|--system] -e | --edit"),
	NULL
};

static GIT_PATH_FUNC(git_path_info_exclude, "info/exclude")

static int add_file_ignore_patterns(const char *filepath, const char **patterns, int count)
{
	FILE *fp;
	int index = 0;
	struct strbuf line_sb = STRBUF_INIT;
	struct string_list patterns_list = STRING_LIST_INIT_NODUP;

	for (index = 0; index < count; index++) {
		struct string_list_item *item = string_list_append(&patterns_list, patterns[index]);
		item->util = NULL; //todo
	}

	fp = fopen_or_warn(filepath, "r");
	if (!fp) {
		strbuf_release(&line_sb);
		string_list_clear(&patterns_list, 0);
		return 1;
	}

	while (strbuf_getline(&line_sb, fp) != EOF) {
		if () {
			//blank line, skip
		}

		for_each_string_list_item(item, &patterns_list) {
			//if item equals the line, mark using item->util
		}
	}

	//todo append all remaining items to ignore file

	strbuf_release(&line_sb);
	string_list_clear(&patterns_list, 0);
	fclose(fp);

	return 0;
}

static int add_ignore_patterns(int scope, const char **patterns, int count)
{
	const char *work_tree = get_git_work_tree();
	struct strbuf path_sb = STRBUF_INIT;

	//todo convert patterns to a list of pattern structs

	if (scope & SCOPE_SHARE) {
		strbuf_addf(&path_sb, "%s/.gitignore", work_tree);
		add_file_ignore_patterns(path_sb.buf, patterns, count);
		strbuf_reset(&path_sb);
	}

	if (scope & SCOPE_LOCAL) {
		strbuf_addf(&path_sb, "%s/%s", work_tree, git_path_info_exclude());
		add_file_ignore_patterns(path_sb.buf, patterns, count);
		strbuf_reset(&path_sb);
	}

	if (scope & SCOPE_SYSTEM) {
		const char *value;
		if (git_config_get_value("core.excludesfile", &value))
			value = xdg_config_home("ignore");
		add_file_ignore_patterns(value, patterns, count);
	}

	strbuf_release(&path_sb);
	return 0;
}

static int remove_ignore_patterns(int scope, const char **patterns, int count)
{
	trace_printf("remove_ignore_patterns %d %d", scope, count);
	return 0;
}

static void edit_file(const char *base, const char *path)
{
	int ret = 0;

	if (base) {
		struct strbuf sb = STRBUF_INIT;
		strbuf_addf(&sb, "%s/%s", base, path);
		ret = launch_editor(sb.buf, NULL, NULL);
		strbuf_release(&sb);
	} else
		ret = launch_editor(path, NULL, NULL);

	if (ret)
		die(_("editing ignore file failed"));
}

static int edit_ignore_file(int scope)
{
	const char *work_tree = get_git_work_tree();

	if (scope & SCOPE_SHARE)
		edit_file(work_tree, ".gitignore");
	if (scope & SCOPE_LOCAL)
		edit_file(work_tree, git_path_info_exclude());

	if (scope & SCOPE_SYSTEM) {
		const char *value;
		if (git_config_get_value("core.excludesfile", &value))
			value = xdg_config_home("ignore");
		edit_file(NULL, value);
	}

	return 0;
}

int cmd_ignore(int argc, const char **argv, const char *prefix)
{
	int scope = 0;
	int action = 0;

	struct option ignore_options[] = {
		OPT_GROUP(N_("Scope")),
		OPT_BIT(0, "share", &scope, N_("write to project's `.gitignore` file"), SCOPE_SHARE),
		OPT_BIT(0, "local", &scope, N_("write to `$GIT_DIR/info/exclude`"), SCOPE_LOCAL),
		OPT_BIT(0, "system", &scope, N_("write to system-wide ignore file"), SCOPE_SYSTEM),
		OPT_GROUP(N_("Action")),
		OPT_BIT(0, "track", &action, N_("add patterns to the ignore file"), ACTION_TRACK),
		OPT_BIT(0, "untrack", &action, N_("remove patterns from the ignore file"), ACTION_UNTRACK),
		OPT_BIT('e', "edit", &action, N_("edit ignore file in editor"), ACTION_EDIT),
		OPT_END()
	};

	if (argc < 2 || !strcmp(argv[1], "-h"))
		usage_with_options(ignore_usage, ignore_options);

	argc = parse_options(argc, argv, prefix, ignore_options, ignore_usage, 0);

	if (action & ACTION_EDIT) {
		if (action & ACTION_TRACK)
			die(_("cannot combine --track and --edit options"));
		if (action & ACTION_UNTRACK)
			die(_("cannot combine --untrack and --edit options"));
	} else {
		if ((action & ACTION_TRACK) && (action & ACTION_UNTRACK))
			die(_("cannot simultaneously add and remove pattern from ignore file"));
		if (argc < 1)
			die(_("must supply at least one pattern"));
	}

	if (!action)
		action |= ACTION_TRACK;
	if (!scope)
		scope |= SCOPE_SHARE;

	if (action & ACTION_TRACK)
		return add_ignore_patterns(scope, argv, argc);
	else if (action & ACTION_UNTRACK)
		return remove_ignore_patterns(scope, argv, argc);
	else if (action & ACTION_EDIT)
		return edit_ignore_file(scope);

	return 1;
}
