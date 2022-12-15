# Problem defenition

Right now **Kubernetes** we are mostly talking about kubectl supports only [SPDY](https://www.chromium.org/spdy/) protocol for command execution and for  [CRI](https://kubernetes.io/docs/setup/production-environment/container-runtimes/#cri-o) interraction.  SPDY was deprecated a few years ago and now should be switched to something else, for example [websockets](https://www.rfc-editor.org/rfc/rfc6455.html).

## Some brief history

This section should help you to understand why  websockets approach still wasn't implemented in current k8s implementation.

### 2015

[SPDY is deprecated. Switch to HTTP/2.](https://github.com/kubernetes/kubernetes/issues/7452), despite "open" PR this better be closed, because firsty the were blocked by GOlang developers [1](https://github.com/golang/go/issues/13444). Nevertheless looks like all faced problems until now have has been resolved, but now they dont have any sort of consesus about HTTP/2 or other possible ways of SPDY deprecation.

### 2020

[kubectl exec/attach using websockets](https://github.com/kubernetes/kubernetes/issues/89163)

In this issue Kubernetes team discussing faced issues with SPDY.
as for now Websockets are highly used in API server and kubelet, for example for version release-1.19, look [here](https://github.com/kubernetes/kubernetes/blob/release-1.19/pkg/kubelet/cri/streaming/remotecommand/websocket.go#L26) for details.


## Current situation

Right now some big parts of k8s are alrerady using websockets; if  you look closely to the source code you will find that main websocket library is a part of standart [golang networking](https://pkg.go.dev/golang.org/x/net) package [websocket](https://pkg.go.dev/golang.org/x/net/websocket), also you can find gorilla/websockets package. In other words k8s community is actively trying to remove SPDY.

# KubeCTL

This package is using SPDY for container control and communication, for example [here](https://github.com/kubernetes/kubernetes/blob/master/staging/src/k8s.io/client-go/tools/remotecommand/remotecommand.go#L31).


There are several tryouts about untroducing websockets into kubetcl, for example: 

[Latest](https://github.com/kubernetes/kubernetes/pull/110142) 

[Previous (less lucky)](https://github.com/kubernetes/kubernetes/pull/107125)

We will discuss only latest PR because this was closest to merge PR.

## Currently faced problems

1) [Flawed exec over websocket protocol](https://github.com/kubernetes/kubernetes/issues/89899) Protocol related issue
2) [Gorilla Toolkit](https://github.com/gorilla) will be [archived](https://docs.github.com/en/repositories/archiving-a-github-repository/archiving-repositories) by the [end of  the 2022](https://github.com/gorilla#gorilla-toolkit).
3) Possible regression if websockets will be implemented in future

Lets discuss these problems in more detailed manner.

### Issue 1  **Flawed exec over websocket protocol**

Problem defenition:

Websocket protocol does not support **half-close** look at [RFC6455](https://www.rfc-editor.org/rfc/rfc6455), exactly to [section-7.1.2](https://www.rfc-editor.org/rfc/rfc6455#section-7.1.2) [section-7.1.6](https://www.rfc-editor.org/rfc/rfc6455#section-7.1.6) for more detailed info, but at the same time SPDY [supports](https://mbelshe.github.io/SPDY-Specification/draft-mbelshe-spdy-00.xml#StreamHalfClose) it.

In case of SPDY this behavior defined by FLAG_FIN, in case of websockets we dont have same posibility out of the box.

### Why this is important

According to STDIN close [issue](https://github.com/kubernetes/kubernetes/issues/89899) having some sort of *half-close* feature would be very handy, because first and last suggestions from declared issue cant solve future problems in future, for more details please look [here](https://github.com/kubernetes/kubernetes/issues/89899#issuecomment-1132502190) and [here](https://github.com/kubernetes/kubernetes/issues/89899#issuecomment-1143026673).

Based on this [websockets protocol enhancements](https://github.com/kubernetes/enhancements/issues/3396) are required, this will allow to fix existing issues, like half-closed like connections without big troubles, and in future it will prevent other possible issues.

### Current protocol update/change related issues

1) Break backward compatibility with old clients
2) Huge amount of refactoring (current implementation is hardly tired to SPDY)

### Issue 2 Gorilla Toolkit deprecation

As I have already mentioned, [Gorilla Toolkit](https://github.com/gorilla) already archived, and we or anyone else **can not** use it in any production scenario, because even in case of (according to paragraph 5 of [CVSS-Specification](https://www.first.org/cvss/v3.1/specification-document)) MEDIUM (4.0-6.9) we cant get a fix from the package developer and we will have to fix it on our end which is bad in terms of user and developer experience.

# Current situation

Because of declared issues K8S developers community finaly made a desigion to drop SPDY and move to websockets [KEP-3396](https://github.com/kubernetes/enhancements/pull/3401/files)

# Some useful links:

[Half closed TCP](https://www.excentis.com/blog/tcp-half-close-a-cool-feature-that-is-now-broken/)
[SPDY Deprecation](https://blog.cloudflare.com/deprecating-spdy/)
[Web Sockets standart]()
