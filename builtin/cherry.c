#include "cache.h"
#include "commit.h"
#include "revision.h"
#include "patch-ids.h"
#include "parse-options.h"
#include "remote.h"

static const char * const cherry_usage[] = {
		N_("git cherry [-v] [<upstream> [<head> [<limit>]]]"),
		NULL
};

// TODO What is this function doing? Should it be renamed? Should it be put somewhere else?
static void get_patch_ids(struct rev_info *rev, struct patch_ids *ids)
{
	struct rev_info check_rev;
	struct commit *commit, *c1, *c2;
	struct object *o1, *o2;
	unsigned flags1, flags2;

	if (rev->pending.nr != 2)
		die(_("need exactly one range"));

	o1 = rev->pending.objects[0].item;
	o2 = rev->pending.objects[1].item;
	flags1 = o1->flags;
	flags2 = o2->flags;
	c1 = lookup_commit_reference(the_repository, &o1->oid);
	c2 = lookup_commit_reference(the_repository, &o2->oid);

	if ((flags1 & UNINTERESTING) == (flags2 & UNINTERESTING))
		die(_("not a range"));

	init_patch_ids(the_repository, ids);

	/* given a range a..b get all patch ids for b..a */
	repo_init_revisions(the_repository, &check_rev, rev->prefix);
	check_rev.max_parents = 1;
	o1->flags ^= UNINTERESTING;
	o2->flags ^= UNINTERESTING;
	add_pending_object(&check_rev, o1, "o1");
	add_pending_object(&check_rev, o2, "o2");
	if (prepare_revision_walk(&check_rev))
		die(_("revision walk setup failed"));

	while ((commit = get_revision(&check_rev)) != NULL)
		add_commit_patch_id(commit, ids);

	/* reset for next revision walk */
	clear_commit_marks(c1, SEEN | UNINTERESTING | SHOWN | ADDED);
	clear_commit_marks(c2, SEEN | UNINTERESTING | SHOWN | ADDED);
	o1->flags = flags1;
	o2->flags = flags2;
}

static int add_pending_commit(const char *arg, struct rev_info *revs, int flags)
{
	struct object_id oid;
	if (get_oid(arg, &oid) == 0) {
		struct commit *commit = lookup_commit_reference(the_repository, &oid);
		if (commit) {
			commit->object.flags |= flags;
			add_pending_object(revs, &commit->object, arg);
			return 0;
		}
	}

	return -1;
}

static void print_commit(char sign, struct commit *commit, int verbose,
		int abbrev, FILE *file)
{
	if (verbose) {
		struct strbuf buf = STRBUF_INIT;
		pp_commit_easy(CMIT_FMT_ONELINE, commit, &buf);
		fprintf(file, "%c %s %s\n", sign,
				find_unique_abbrev(&commit->object.oid, abbrev),
				buf.buf);
		strbuf_release(&buf);
	} else {
		fprintf(file, "%c %s\n", sign,
				find_unique_abbrev(&commit->object.oid, abbrev));
	}
}

int cmd_cherry(int argc, const char **argv, const char *prefix)
{
	struct rev_info revs;
	struct patch_ids ids;
	struct commit *commit;
	struct commit_list *list = NULL;
	struct branch *current_branch;
	const char *upstream;
	const char *head = "HEAD";
	const char *limit = NULL;
	int verbose = 0, abbrev = 0;

	struct option options[] = {
			OPT__ABBREV(&abbrev),
			OPT__VERBOSE(&verbose, N_("be verbose")),
			OPT_END()
	};

	argc = parse_options(argc, argv, prefix, options, cherry_usage, 0);
	switch (argc) {
		case 3:
			limit = argv[2];
			/* FALLTHROUGH */
		case 2:
			head = argv[1];
			/* FALLTHROUGH */
		case 1:
			upstream = argv[0];
			break;
		default:
			current_branch = branch_get(NULL);
			upstream = branch_get_upstream(current_branch, NULL);
			if (!upstream) {
				error(_("Current branch does not appear to be tracking any "
						"upstream branch.\nPlease specify an <upstream>."));
				usage_with_options(cherry_usage, options);
			}
	}

	repo_init_revisions(the_repository, &revs, prefix);
	revs.max_parents = 1;

	if (add_pending_commit(head, &revs, 0))
		die(_("unknown commit %s"), head);
	if (add_pending_commit(upstream, &revs, UNINTERESTING))
		die(_("unknown commit %s"), upstream);

	/* Don't say anything if head and upstream are the same. */
	if (revs.pending.nr == 2) {
		struct object_array_entry *o = revs.pending.objects;
		if (oideq(&o[0].item->oid, &o[1].item->oid))
			return 0;
	}

	get_patch_ids(&revs, &ids);

	if (limit && add_pending_commit(limit, &revs, UNINTERESTING))
		die(_("unknown commit %s"), limit);

	/* reverse the list of commits */
	if (prepare_revision_walk(&revs))
		die(_("revision walk setup failed"));
	while ((commit = get_revision(&revs)) != NULL)
		commit_list_insert(commit, &list);

	while (list) {
		char sign = '+';

		commit = list->item;
		if (has_commit_patch_id(commit, &ids))
			sign = '-';

		print_commit(sign, commit, verbose, abbrev, revs.diffopt.file);
		list = list->next;
	}

	free_patch_ids(&ids);
	return 0;
}
