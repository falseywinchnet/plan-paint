# CONV provenance

Native port of the website demonstrator `web/exhibits/compact-ordered-nodal-variation/conv-kernel.c`. Source SHA-256: `94d226d7bb7b86d7fb55ac64c267779eb3bd2cb3736bb990e0f10a05e859ab47`. Retains witnessed sign transport, least-distance signed-current projection, quintic Bernstein reconstruction, endpoint nodal enlargement and integrated basin reduction. Double precision; no fast-math. Scratch belongs to each resampling invocation. Short lines below five nodes use explicit linear interpolation/area integration, because the five-node jet stencil is not defined there.
