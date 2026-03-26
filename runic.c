//Stan Alexandra 312CD
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MY_PI 3.14159265358979323846
//rule structure for l-systems
typedef struct {
	char predecessor;
	char successor[64];
} rule;

//rgb pixel structure
typedef struct {
	unsigned char r, g, b;
} pixel;

//image structure
typedef struct {
	int w, h;
	pixel **data;
} image;

//snapshot of the current state
typedef struct {
	char *axiom;
	rule *rules;
	int nrules;
	int loaded;
	image img_copy;
	int has_img;
	int last_was_turtle;
	int last_was_lsystem;
	char filename[256];
} lstate;

//turtle graphics state structure
typedef struct {
	double x;
	double y;
	double angle;
} turtle;

//glyph structure
typedef struct {
	int encoding;
	int width, height;
	int x, y;
	int dwx;
	unsigned char **bitmap;
} glyph;

//bitmap font structure
typedef struct {
	char name[65536];
	glyph *glyphs;
	int nglyphs;
} bdf_font;

//custom sine and cosine functions using taylor series
//because i cant include math.h in the makefile
double custom_sin(double x)
{
	double t, s;
	int i;
	x = x - (2.0 * MY_PI) * (long long)(x / (2.0 * MY_PI));
	if (x < 0)
		x += 2.0 * MY_PI;
	if (x > MY_PI)
		x -= 2.0 * MY_PI;
	t = x;
	s = x;
	for (i = 1; i < 30; i++) {
		t = t * (-1.0) * x * x / ((2.0 * i) * (2.0 * i + 1.0));
		s += t;
	}
	return s;
}

double custom_cos(double x)
{
	return custom_sin(x + (MY_PI / 2.0));
}

//helper to reduce deep indentation in cmd_font bitmap processing
void process_glyph_bitmap(FILE *f, glyph *g)
{
	int r, c;
	char hex_pair[3] = {0};
	unsigned int byte_val = 0;
	static char line[65536];

	g->bitmap = malloc(g->height * sizeof(unsigned char *));
	for (r = 0; r < g->height; r++) {
		g->bitmap[r] = calloc(g->width, sizeof(unsigned char));
		if (!fgets(line, sizeof(line), f))
			break;
		char *p = line;
		while (*p == ' ' || *p == '\t')
			p++;
		for (c = 0; c < g->width; c++) {
			//read new byte every 8 bits
			if (c % 8 == 0) {
				hex_pair[0] = p[0];
				hex_pair[1] = p[1];
				hex_pair[2] = '\0';
				if (sscanf(hex_pair, "%x", &byte_val) != 1)
					byte_val = 0;
				p += 2;
			}
			//set bit in bitmap
			if (byte_val & (1 << (7 - (c % 8))))
				g->bitmap[r][c] = 1;
		}
	}
}

//free image memory
void free_image(image *img)
{
	int i;
	for (i = 0; i < img->h; i++)
		free(img->data[i]);
	free(img->data);
}

//free snapshot content
void free_state_content(lstate *state)
{
	if (state->axiom) {
		free(state->axiom);
		state->axiom = NULL;
	}
	if (state->rules) {
		free(state->rules);
		state->rules = NULL;
	}
	if (state->has_img) {
		free_image(&state->img_copy);
		state->has_img = 0;
	}
}

//dealocates font glyphs and bitmap arrays
void free_current_font(bdf_font *f)
{
	int i, j;
	for (i = 0; i < f->nglyphs; i++) {
		for (j = 0; j < f->glyphs[i].height; j++)
			free(f->glyphs[i].bitmap[j]);
		free(f->glyphs[i].bitmap);
	}
	if (f->glyphs)
		free(f->glyphs);
	f->glyphs = NULL;
	f->nglyphs = 0;
}

//iterarively cleans the stack from top to bottom
void clear_stack(lstate *stack, int *ptr)
{
	while (*ptr > 0)
		free_state_content(&stack[--(*ptr)]);
}

//safe duplicate string function
char *strdup_safe(const char *s)
{
	size_t n;
	char *p;
	if (!s)
		return NULL;
	n = strlen(s) + 1;
	p = malloc(n);
	if (!p)
		return NULL;
	memcpy(p, s, n);
	return p;
}

//create a new image of given dimensions
image create_image(int width, int height)
{
	image img;
	int i;
	img.w = width;
	img.h = height;
	img.data = malloc(height * sizeof(pixel *));
	for (i = 0; i < height; i++)
		img.data[i] = malloc(width * sizeof(pixel));
	return img;
}

//copy an image
image copy_image(image src)
{
	image dest = create_image(src.w, src.h);
	int i;
	for (i = 0; i < src.h; i++)
		memcpy(dest.data[i], src.data[i], src.w * sizeof(pixel));
	return dest;
}

//create a snapshot, like a history state
lstate create_snapshot(char *ax, rule *rs, int nr, int ld,
					   image *curr, int i_ld, int tur,
					   int lsys, char *fname)
{
	lstate snap;
	snap.axiom = ax ? strdup_safe(ax) : NULL;
	snap.nrules = nr;
	snap.loaded = ld;
	snap.rules = malloc(sizeof(rule) * 256);
	if (snap.rules && rs)
		memcpy(snap.rules, rs, sizeof(rule) * 256);
	snap.last_was_turtle = tur;
	snap.last_was_lsystem = lsys;
	if (fname) {
		strncpy(snap.filename, fname, 255);
		snap.filename[255] = '\0';
	} else {
		snap.filename[0] = '\0';
	}
	snap.has_img = i_ld;
	if (i_ld)
		snap.img_copy = copy_image(*curr);
	return snap;
}

//replaces active variables with snapshot content
void restore_snapshot(lstate snap, char **axiom, rule *rules, int *nrules,
					  int *loaded, image *current_img, int *img_loaded)
{
	if (*axiom)
		free(*axiom);
	*axiom = snap.axiom;
	*nrules = snap.nrules;
	*loaded = snap.loaded;
	if (snap.rules) {
		memcpy(rules, snap.rules, sizeof(rule) * 256);
		free(snap.rules);
	}
	if (*img_loaded)
		free_image(current_img);
	*img_loaded = snap.has_img;
	if (snap.has_img)
		*current_img = snap.img_copy;
}

//parses l-system file to load axiom and rules
int handle_lsystem(rule *rules, int *nrules, char **axiom, int *loaded,
				   char *filename)
{
	FILE *f = fopen(filename, "r");
	char temp_axiom[2048];
	int i;
	if (!f) {
		printf("Failed to load %s\n", filename);
		return 0;
	}
	if (fscanf(f, "%2047s", temp_axiom) != 1) {
		fclose(f);
		printf("Failed to load %s\n", filename);
		return 0;
	}
	if (*axiom)
		free(*axiom);
	*axiom = strdup_safe(temp_axiom);
	if (fscanf(f, "%d", nrules) != 1)
		*nrules = 0;
	for (i = 0; i < *nrules && i < 256; ++i) {
		char symb[16], succ[64];
		if (fscanf(f, "%s %s", symb, succ) == 2) {
			rules[i].predecessor = symb[0];
			strncpy(rules[i].successor, succ, 63);
			rules[i].successor[63] = '\0';
		}
	}
	fclose(f);
	*loaded = 1;
	printf("Loaded %s (L-system with %d rules)\n", filename, *nrules);
	return 1;
}

//derives the l-system for a given number of steps
void handle_derive(rule *rules, int nrules, char *axiom, int loaded)
{
	int steps = 0, s, r;
	size_t i;
	char *current, *next;
	if (scanf("%d", &steps) != 1)
		return;
	if (!loaded) {
		printf("No L-system loaded\n");
		return;
	}
	current = strdup_safe(axiom);
	for (s = 0; s < steps; ++s) {
		size_t cur_len = strlen(current);
		size_t capacity = cur_len * 2 + 1024;
		size_t next_len = 0;
		next = malloc(capacity);
		if (!next) {
			free(current);
			return;
		}
		for (i = 0; i < cur_len; ++i) {
			int found = -1;
			for (r = 0; r < nrules; r++) {
				if (rules[r].predecessor == current[i]) {
					found = r;
					break;
				}
			}
			if (found != -1) {
				size_t slen = strlen(rules[found].successor);
				if (next_len + slen >= capacity) {
					capacity = (next_len + slen) * 2 + 1024;
					next = realloc(next, capacity);
				}
				memcpy(next + next_len, rules[found].successor, slen);
				next_len += slen;
			} else {
				if (next_len + 1 >= capacity) {
					capacity = capacity * 2 + 1024;
					next = realloc(next, capacity);
				}
				next[next_len++] = current[i];
			}
		}
		next[next_len] = '\0';
		free(current);
		current = next;
		if (next_len > 10000000)
			break;
	}
	printf("%s\n", current);
	free(current);
}

//handles lsystem command
void cmd_lsystem(char **ax, rule *rules, int *nr, int *ld,
				 image *img, int i_ld, char *fn, lstate *u_stk,
				 int *u_ptr, lstate *r_stk, int *r_ptr)
{
	char temp_name[256];
	if (scanf("%255s", temp_name) == 1) {
		lstate old = create_snapshot(*ax, rules, *nr, *ld, img, i_ld, 0, 1, fn);
		if (handle_lsystem(rules, nr, ax, ld, temp_name)) {
			strncpy(fn, temp_name, 255);
			u_stk[(*u_ptr)++] = old;
			clear_stack(r_stk, r_ptr);
		} else {
			free_state_content(&old);
		}
	}
}

//writes image data to a binary ppm file
void save_ppm(const char *filename, image *img)
{
	FILE *f = fopen(filename, "wb");
	int i;
	if (!f)
		return;
	fprintf(f, "P6\n%d %d\n255\n", img->w, img->h);
	for (i = 0; i < img->h; i++)
		fwrite(img->data[i], 3, img->w, f);
	fclose(f);
}

//bresenham's line drawing algorithm
void draw_line(image *img, double x0_d, double y0_d, double x1_d, double y1_d,
			   pixel color)
{
	int x0 = (x0_d >= 0) ? (int)(x0_d + 0.5) : (int)(x0_d - 0.5);
	int y0 = (y0_d >= 0) ? (int)(y0_d + 0.5) : (int)(y0_d - 0.5);
	int x1 = (x1_d >= 0) ? (int)(x1_d + 0.5) : (int)(x1_d - 0.5);
	int y1 = (y1_d >= 0) ? (int)(y1_d + 0.5) : (int)(y1_d - 0.5);
	int dx = abs(x1 - x0), sx = (x0 < x1) ? 1 : -1;
	int dy = -abs(y1 - y0), sy = (y0 < y1) ? 1 : -1;
	int err = dx + dy, e2;
	while (1) {
		if (x0 >= 0 && x0 < img->w && y0 >= 0 && y0 < img->h)
			img->data[img->h - 1 - y0][x0] = color;
		if (x0 == x1 && y0 == y1)
			break;
		e2 = 2 * err;
		if (e2 >= dy) {
			err += dy;
			x0 += sx;
		}
		if (e2 <= dx) {
			err += dx;
			y0 += sy;
		}
	}
}

//interperts l-system string as turtle instructions
void execute_turtle(image *img, const char *str, double dist, double delta,
					turtle start, pixel p)
{
	turtle t = start;
	turtle stack[4096];
	int top = 0, i;
	for (i = 0; str[i]; i++) {
		if (str[i] == 'F' || str[i] == 'f') {
			double rad = t.angle * (MY_PI / 180.0);
			double nx = t.x + dist * custom_cos(rad);
			double ny = t.y + dist * custom_sin(rad);
			if (str[i] == 'F')
				draw_line(img, t.x, t.y, nx, ny, p);
			t.x = nx;
			t.y = ny;
		} else if (str[i] == '+') {
			t.angle += delta;
		} else if (str[i] == '-') {
			t.angle -= delta;
		} else if (str[i] == '[') {
			stack[top++] = t;
		} else if (str[i] == ']' && top > 0) {
			t = stack[--top];
		}
	}
}

//derives l-system and return resulting string, used in turtle command
//it is similar to handle_derive but returns the string instead of printing it
char *run_derive_to_string(char *axiom, rule *rules, int nrules, int steps)
{
	char *current, *next, *temp;
	int s, r;
	size_t i;
	if (!axiom)
		return NULL;
	current = strdup_safe(axiom);
	if (!current)
		return NULL;
	for (s = 0; s < steps; ++s) {
		size_t cur_len = strlen(current);
		size_t capacity = cur_len * 2 + 1024;
		size_t next_len = 0;
		next = malloc(capacity);
		if (!next) {
			free(current);
			return NULL;
		}
		for (i = 0; i < cur_len; ++i) {
			int found = -1;
			for (r = 0; r < nrules; r++) {
				if (rules[r].predecessor == current[i]) {
					found = r;
					break;
				}
			}
			if (found != -1) {
				size_t slen = strlen(rules[found].successor);
				if (next_len + slen >= capacity) {
					capacity = (next_len + slen) * 2 + 1024;
					temp = realloc(next, capacity);
					if (!temp) {
						free(next);
						free(current);
						return NULL;
					}
					next = temp;
				}
				memcpy(next + next_len, rules[found].successor, slen);
				next_len += slen;
			} else {
				if (next_len + 1 >= capacity) {
					capacity = capacity * 2 + 1024;
					temp = realloc(next, capacity);
					if (!temp) {
						free(next);
						free(current);
						return NULL;
					}
					next = temp;
				}
				next[next_len++] = current[i];
			}
		}
		next[next_len] = '\0';
		free(current);
		current = next;
		if (next_len > 5000000)
			break;
	}
	return current;
}

//loads a ppm binary or text image into memory
image load_ppm(const char *filename)
{
	FILE *f = fopen(filename, "rb");
	image img = {0, 0, NULL};
	char magic[3];
	int c, max_val, i, j;
	if (!f)
		return img;
	if (fscanf(f, "%2s", magic) != 1) {
		fclose(f);
		return img;
	}
	c = fgetc(f);
	while (c != EOF) {
		if (c == '#') {
			while ((c = fgetc(f)) != '\n' && c != EOF)
				;
		} else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
			c = fgetc(f);
		} else {
			ungetc(c, f);
			break;
		}
	}
	if (fscanf(f, "%d %d %d", &img.w, &img.h, &max_val) != 3) {
		fclose(f);
		return img;
	}
	fgetc(f);
	img.data = malloc(img.h * sizeof(pixel *));
	for (i = 0; i < img.h; i++) {
		img.data[i] = malloc(img.w * sizeof(pixel));
		if (!strcmp(magic, "P6")) {
			fread(img.data[i], sizeof(pixel), img.w, f);
		} else {
			for (j = 0; j < img.w; j++) {
				int r, g, b;
				if (fscanf(f, "%d %d %d", &r, &g, &b) == 3) {
					img.data[i][j].r = (unsigned char)r;
					img.data[i][j].g = (unsigned char)g;
					img.data[i][j].b = (unsigned char)b;
				}
			}
		}
	}
	fclose(f);
	return img;
}

//handles turtle command
void cmd_turtle(char *ax, rule *rs, int nr, int ld, image *img,
				int i_ld, char *fn, lstate *u_stk, int *u_ptr,
				lstate *r_stk, int *r_ptr)
{
	double tx, ty, tdist, tangle, tdelta;
	int r, g, b, tsteps;
	if (scanf("%lf %lf %lf %lf %lf %d %d %d %d", &tx, &ty, &tdist, &tangle,
			  &tdelta, &tsteps, &r, &g, &b) == 9) {
		if (ld && i_ld) {
			u_stk[(*u_ptr)++] = create_snapshot(ax, rs, nr, ld, img,
												i_ld, 1, 0, fn);
			clear_stack(r_stk, r_ptr);
			char *gen = run_derive_to_string(ax, rs, nr, tsteps);
			if (gen) {
				pixel color = {(unsigned char)r, (unsigned char)g,
							   (unsigned char)b};
				turtle t_start = {tx, ty, tangle};
				execute_turtle(img, gen, tdist, tdelta, t_start, color);
				printf("Drawing done\n");
				free(gen);
			}
		} else {
			printf(!i_ld ? "No image loaded\n" : "No L-system loaded\n");
		}
	}
}

//parses bdf font file to load bitmap glyph data for text rendering
void cmd_font(char *path, bdf_font *current_font, int *font_loaded)
{
	FILE *f = fopen(path, "r");
	static char line[65536];
	int nglyphs = 0, g_idx = 0;
	if (!f) {
		printf("Failed to load %s\n", path);
		return;
	}
	if (*font_loaded)
		free_current_font(current_font);
	while (fgets(line, sizeof(line), f)) {
		if (strncmp(line, "FONT ", 5) == 0) {
			char *nd = line + strlen(line) - 1;
			while (nd > line && (*nd == '\n' || *nd == '\r' || *nd == ' ')) {
				*nd = '\0';
				nd--;
			}
			strncpy(current_font->name, line + 5, 65535);
		}
		if (strncmp(line, "CHARS ", 6) == 0) {
			if (sscanf(line + 6, "%d", &nglyphs) == 1)
				current_font->glyphs = calloc(nglyphs, sizeof(glyph));
		}
		if (strncmp(line, "STARTCHAR", 9) == 0) {
			glyph *g = &current_font->glyphs[g_idx];
			while (fgets(line, sizeof(line), f) &&
				   strncmp(line, "ENDCHAR", 7) != 0) {
				if (strncmp(line, "ENCODING", 8) == 0)
					if (sscanf(line + 9, "%d", &g->encoding) != 1)
						g->encoding = 0;
				if (strncmp(line, "DWIDTH", 6) == 0)
					if (sscanf(line + 7, "%d", &g->dwx) != 1)
						g->dwx = 0;
				if (strncmp(line, "BBX", 3) == 0)
					if (sscanf(line + 4, "%d %d %d %d", &g->width,
							   &g->height, &g->x, &g->y) != 4)
						g->width = 0;
				if (strncmp(line, "BITMAP", 6) == 0)
					process_glyph_bitmap(f, g);
			}
			g_idx++;
		}
	}
	fclose(f);
	current_font->nglyphs = nglyphs;
	*font_loaded = 1;
	printf("Loaded %s (bitmap font %s)\n", path, current_font->name);
}

//renders text using current font, mapping glyph bitmaps to the image
void cmd_type(image *img, int img_ld, bdf_font *f, int font_ld)
{
	char text[1024] = "";
	int start_x, start_y, r, g, b, i, j, char_idx;
	pixel color;

	scanf(" \"%[^\"]\"", text);
	scanf("%d %d %d %d %d", &start_x, &start_y, &r, &g, &b);

	if (!img_ld) {
		printf("No image loaded\n");
		return;
	}
	if (!font_ld) {
		printf("No font loaded\n");
		return;
	}

	color.r = (unsigned char)r;
	color.g = (unsigned char)g;
	color.b = (unsigned char)b;

	int current_x = start_x;
	if (text[0] == '\0') {
		printf("Text written\n");
		return;
	}
	for (i = 0; text[i]; i++) {
		char_idx = -1;
		for (j = 0; j < f->nglyphs; j++) {
			if (f->glyphs[j].encoding == (int)text[i]) {
				char_idx = j;
				break;
			}
		}

		if (char_idx != -1) {
			glyph *gl = &f->glyphs[char_idx];
			for (int row = 0; row < gl->height; row++) {
				for (int col = 0; col < gl->width; col++) {
					if (gl->bitmap[row][col]) {
						int px = current_x + gl->x + col;
						int py = start_y + gl->y + (gl->height - 1 - row);
						(px >= 0 && px < img->w && py >= 0 &&
						 py < img->h) ?
						(img->data[img->h - 1 - py][px] = color) : (void)0;
					}
				}
			}
			current_x += gl->dwx;
		}
	}
	printf("Text written\n");
}

//moves program state back by popping from undo stack and pushing to redo
int cmd_undo(char **ax, rule *rs, int *nr, int *ld, image *img,
			 int *i_ld, char *fn, lstate *u_stk, int *u_ptr,
			 lstate *r_stk, int *r_ptr)
{
	if (*u_ptr > 0) {
		lstate to_undo = u_stk[--(*u_ptr)];
		int action_type = to_undo.last_was_turtle;
		char redo_fname[256];
		if (to_undo.last_was_turtle == 3) {
			strncpy(redo_fname, to_undo.filename, 255);
		} else if (fn && fn[0] != '\0') {
			strncpy(redo_fname, fn, 255);
		} else {
			strncpy(redo_fname, to_undo.filename, 255);
		}
		redo_fname[255] = '\0';
		r_stk[(*r_ptr)++] = create_snapshot(*ax, rs, *nr, *ld, img,
											*i_ld, to_undo.last_was_turtle,
											to_undo.last_was_lsystem,
											redo_fname);
		restore_snapshot(to_undo, ax, rs, nr, ld, img, i_ld);
		strncpy(fn, to_undo.filename, 255);
		return action_type;
	}
	printf("Nothing to undo\n");
	return -1;
}

//moves program state forward by popping from redo stack and pusing to undo
void cmd_redo(char **ax, rule *rs, int *nr, int *ld, image *img,
			  int *i_ld, char *fn, lstate *u_stk, int *u_ptr,
			  lstate *r_stk, int *r_ptr, bdf_font *f_st, int *f_ld)
{
	if (*r_ptr > 0) {
		lstate to_res = r_stk[--(*r_ptr)];
		int type = to_res.last_was_turtle;
		u_stk[(*u_ptr)++] = create_snapshot(*ax, rs, *nr, *ld, img,
											*i_ld, type,
											to_res.last_was_lsystem,
											to_res.filename);
		restore_snapshot(to_res, ax, rs, nr, ld, img, i_ld);
		if (type == 3) {
			cmd_font(to_res.filename, f_st, f_ld);
		} else if (type == 5) {
			image n_img = load_ppm(to_res.filename);
			if (n_img.data) {
				if (*i_ld)
					free_image(img);
				*img = n_img;
				*i_ld = 1;
				printf("Loaded %s (PPM image %dx%d)\n",
					   to_res.filename, img->w, img->h);
			} else {
				printf("Failed to load %s\n", to_res.filename);
			}
		} else {
			strncpy(fn, to_res.filename, 255);
			if (type == 1)
				printf("Drawing done\n");
			else if (type == 2)
				printf("Text written\n");
			else if (to_res.last_was_lsystem == 1)
				printf("Loaded %s (L-system with %d rules)\n",
					   to_res.filename, *nr);
		}
	} else {
		printf("Nothing to redo\n");
	}
}

//helper function to handle primary non-drawing state commands
int handle_core_cmds(char *cmd, char **ax, rule *rs, int *nr, int *ld,
					 image *img, int *i_ld, char *fn, lstate *u_stk,
					 int *u_ptr, lstate *r_stk, int *r_ptr,
					 bdf_font *font, int *f_ld)
{
	if (!strcmp(cmd, "LSYSTEM")) {
		cmd_lsystem(ax, rs, nr, ld, img, *i_ld, fn, u_stk, u_ptr, r_stk, r_ptr);
		return 1;
	}
	if (!strcmp(cmd, "DERIVE")) {
		handle_derive(rs, *nr, *ax, *ld);
		return 1;
	}
	if (!strcmp(cmd, "UNDO")) {
		if (*u_ptr > 0) {
			int act = cmd_undo(ax, rs, nr, ld, img, i_ld, fn,
							   u_stk, u_ptr, r_stk, r_ptr);
			if (act == 3 && *f_ld) {
				free_current_font(font);
				*f_ld = 0;
			}
		} else {
			printf("Nothing to undo\n");
		}
		return 1;
	}
	if (!strcmp(cmd, "REDO")) {
		cmd_redo(ax, rs, nr, ld, img, i_ld, fn, u_stk, u_ptr, r_stk, r_ptr,
				 font, f_ld);
		return 1;
	}
	return 0;
}

//retrives value of a specific bit from the global ppm image
int get_bit(image *img, long i)
{
	long pixel_idx = i / 24;
	int bit_in_pixel = i % 24;
	int row = pixel_idx / img->w;
	int col = pixel_idx % img->w;
	int channel = bit_in_pixel / 8; //0=R, 1=G, 2=B
	int bit_offset = 7 - (bit_in_pixel % 8);
	unsigned char val = 0;

	if (channel == 0)
		val = img->data[row][col].r;
	else if (channel == 1)
		val = img->data[row][col].g;
	else
		val = img->data[row][col].b;

	return (val >> bit_offset) & 1;
}

//checks if a 4-bit sequence matches
int check_sequence(image *img, long start, long total_bits)
{
	if (start + 3 >= total_bits)
		return 0;
	int b0 = get_bit(img, start);
	int b1 = get_bit(img, start + 1);
	int b2 = get_bit(img, start + 2);
	int b3 = get_bit(img, start + 3);

	if (b0 == 0 && b1 == 0 && b2 == 1 && b3 == 0)
		return 1;
	if (b0 == 1 && b1 == 1 && b2 == 0 && b3 == 1)
		return 1;
	return 0;
}

//calculates and prints potential engraving adaptor signal errors
void print_bit_warning(image *img, long error_bit_idx)
{
	long p_idx = error_bit_idx / 24;
	int col = (int)(p_idx % img->w);
	int row_in_mem = (int)(p_idx / img->w);
	int y_math = img->h - 1 - row_in_mem;
	pixel p = img->data[row_in_mem][col];
	int bit_in_p = (int)(error_bit_idx % 24);
	int chan = bit_in_p / 8;
	int off = 7 - (bit_in_p % 8);

	if (chan == 0)
		p.r ^= (1 << off);
	else if (chan == 1)
		p.g ^= (1 << off);
	else
		p.b ^= (1 << off);

	printf("Warning: pixel at (%d, %d) may be read as (%u, %u, %u)\n",
		   col, y_math, p.r, p.g, p.b);
}

//scans image bit by bit interference pattern sequences
void cmd_bitcheck(image *img, int img_ld)
{
	long i, total_bits;

	if (!img_ld) {
		printf("No image loaded\n");
		return;
	}
	total_bits = (long)img->w * img->h * 24;
	for (i = 0; i <= total_bits - 4; i++) {
		if (check_sequence(img, i, total_bits)) {
			print_bit_warning(img, i + 2);
		}
	}
}

int main(void)
{
	char cmd[64], *axiom = NULL, current_filename[256] = "";
	rule *rules = malloc(sizeof(rule) * 256);
	int nrules = 0, loaded = 0, u_ptr = 0, r_ptr = 0, img_loaded = 0;
	lstate u_stack[2000], r_stack[2000];
	image img = {0, 0, NULL};
	bdf_font current_font = {"", NULL, 0};
	int font_loaded = 0;
	while (scanf("%63s", cmd) == 1) {
		if (!strcmp(cmd, "EXIT"))
			break;
		handle_core_cmds(cmd, &axiom, rules, &nrules, &loaded, &img,
						 &img_loaded, current_filename, u_stack, &u_ptr,
						 r_stack, &r_ptr, &current_font, &font_loaded);
		if (!strcmp(cmd, "LOAD")) {
			char path[256];
			if (scanf("%255s", path) == 1) {
				lstate old = create_snapshot(axiom, rules, nrules, loaded,
											 &img, img_loaded, 5, 0, path);
				clear_stack(r_stack, &r_ptr);
				image n_img = load_ppm(path);
				if (n_img.data) {
					u_stack[u_ptr++] = old;
					if (img_loaded)
						free_image(&img);
					img = n_img;
					img_loaded = 1;
					printf("Loaded %s (PPM image %dx%d)\n", path, img.w, img.h);
				} else {
					free_state_content(&old);
					printf("Failed to load %s\n", path);
				}
			}
		} else if (!strcmp(cmd, "SAVE")) {
			char path[256];
			if (scanf("%255s", path) == 1 && img_loaded) {
				save_ppm(path, &img);
				printf("Saved %s\n", path);
			}
		} else if (!strcmp(cmd, "TURTLE")) {
			cmd_turtle(axiom, rules, nrules, loaded, &img,
					   img_loaded, current_filename, u_stack,
					   &u_ptr, r_stack, &r_ptr);
		} else if (!strcmp(cmd, "FONT")) {
			char path[256];
			if (scanf("%255s", path) == 1) {
				u_stack[u_ptr++] = create_snapshot(axiom, rules, nrules,
												   loaded, &img, img_loaded,
												   3, 0, path);
				clear_stack(r_stack, &r_ptr);
				cmd_font(path, &current_font, &font_loaded);
			}
		} else if (!strcmp(cmd, "TYPE")) {
			if (img_loaded && font_loaded) {
				lstate sn = create_snapshot(axiom, rules, nrules,
											loaded, &img, img_loaded,
											2, 0, current_filename);
				u_stack[u_ptr++] = sn;
				clear_stack(r_stack, &r_ptr);
				cmd_type(&img, img_loaded, &current_font, font_loaded);
			} else {
				char dm[1024];
				int d1, d2, d3, d4, d5;
				scanf(" \"%[^\"]\"", dm);
				if (scanf("%d %d %d %d %d", &d1, &d2, &d3, &d4, &d5) == 5)
					printf(!img_loaded ? "No image loaded\n" :
						   "No font loaded\n");
			}
		} else if (!strcmp(cmd, "BITCHECK"))
			cmd_bitcheck(&img, img_loaded);
	}
	if (font_loaded)
		free_current_font(&current_font);
	if (axiom)
		free(axiom);
	free(rules);
	if (img_loaded)
		free_image(&img);
	clear_stack(u_stack, &u_ptr); clear_stack(r_stack, &r_ptr);
	return 0;
}
