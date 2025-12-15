#include "Schemas/schemas.h"

bool isVariable(Schema s) { return s.v > 0; }
bool isEmpty(Schema s) { return s.v == 0 && s.arity == 0; }

void initSchema(Schema *s, unsigned arity) {
    s->v = 0;
    s->arity = arity;
    if (arity) {
        s->subschemas = malloc(arity * sizeof(*(s->subschemas)));
    }
}

