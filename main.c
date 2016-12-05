#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "json.h"

int main()
{
	json_t *root = json_create_object();
	printf("%d\n", json_get_array_size(root));

	char *str = json_print(root);
	printf("%s\n", str);

	json_add_number_to_object(root, "type", 1);
	json_add_string_to_object(root, "mac", "00117F934A02");

	str = json_print_unformatted(root);
	printf("%s\n", str);
	printf("%d\n", json_get_array_size(root));

	free(str);

	str = json_print(root);
	printf("%s\n", str);

	free(str);

	json_delete(root);

	return 0;
}
