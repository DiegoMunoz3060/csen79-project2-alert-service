## Project 2: alert service

The simulated clock starts at day 1, 00:00, and only `TICK` moves it. Times print as
`D<day> <HH:MM>`, for example `D1 08:00` or `D3 17:30`.

Topics are paths of words separated by `/`, such as `news/sports/soccer`. A subscription
to a topic also covers every topic below it: `news` covers `news/sports/soccer`.

| Command | Output |
| --- | --- |
| `SUB <user> <topic>` | `OK SUB <user> <topic>`. Alerts for this topic are delivered as soon as they are published |
| `SUB <user> <topic> at <HH:MM>` | Same output. Publications are held and delivered together every day at that time, starting with the first such time after now |
| `SUB <user> <topic> every <hours>` | Same output. Publications are held and delivered together every `hours` hours, the first time `hours` hours after now |
| `SUB <user> <item> below <price>` | Same output. The user is alerted when the item's price drops below `price`; see PRICE |
| `UNSUB <user> <topic>` | `OK UNSUB <user> <topic>`, or `ERROR not subscribed <user> <topic>`. Held publications for it are discarded |
| `PUB <topic> <message>` | One `ALERT` line per user delivered right now. The message is the rest of the line |
| `PRICE <item> <price>` | One `ALERT` line per user whose threshold this price change crossed |
| `TICK <minutes>` | One `ALERT` line per held publication delivered during that time, then `TIME <new time>` |
| `INBOX <user> [n]` | The user's last n alerts (default K), oldest first, as `ALERT` lines, then `END` |
| `HISTORY <user> [n]` | That user's events, oldest first, then `END`. With n, only the last n events |
| `STATS` | Your own counters, each line starting with `STATS ` |

Alert lines:

```
ALERT <time> <user> <topic> <message>
ALERT <time> <user> <item> price <new price> below <threshold>
```

Delivery rules:

- A second subscription by the same user to the same topic replaces the first and discards its held publications.
  Combining `below` with `at` or `every` is `ERROR bad command`.
- `PUB` reaches each user at most once. If several of a user's subscriptions cover the topic,
  the most specific one (the longest topic) decides whether the alert is sent now or held.
- `below` subscriptions only react to `PRICE` on exactly that item, never to `PUB`.
- A `PRICE` alerts a `below <t>` subscriber when the price drops strictly below t.
  The subscription is removed after it fires; the user must subscribe again for another alert.
- Order of ALERT lines for one `PUB`: by when the deciding subscription was made, oldest
  first. For one `PRICE`: by threshold, highest first, then by when the subscription was made.
- During `TICK`, deliveries happen in time order; deliveries due at the same minute go in the
  order they were scheduled. Each delivery prints its held publications in the
  order they were published, all stamped with the delivery time. A delivery with nothing
  held prints nothing.
- A publication made at the same minute as a scheduled delivery that has already happened
  waits for the next one.
- Each user's inbox holds the last K alerts. K is 10 unless set with `--inbox K` on the
  command line.

`HISTORY` event lines, which are kept in full even after the inbox drops old alerts:

```
<time> SUB <topic> [below <price> | at <HH:MM> | every <hours>]   (as typed)
<time> UNSUB <topic>
<time> ALERT <topic> <message>
```
